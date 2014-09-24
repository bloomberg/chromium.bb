// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/thumbnail/thumbnail_store.h"

#include <algorithm>
#include <cmath>

#include "base/big_endian.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "content/public/browser/android/ui_resource_provider.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/android_opengl/etc1/etc1.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace {

const float kApproximationScaleFactor = 4.f;
const base::TimeDelta kCaptureMinRequestTimeMs(
    base::TimeDelta::FromMilliseconds(1000));

const int kCompressedKey = 0xABABABAB;
const int kCurrentExtraVersion = 1;

// Indicates whether we prefer to have more free CPU memory over GPU memory.
const bool kPreferCPUMemory = true;

size_t NextPowerOfTwo(size_t x) {
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

size_t RoundUpMod4(size_t x) {
  return (x + 3) & ~3;
}

gfx::Size GetEncodedSize(const gfx::Size& bitmap_size, bool supports_npot) {
  DCHECK(!bitmap_size.IsEmpty());
  if (!supports_npot)
    return gfx::Size(NextPowerOfTwo(bitmap_size.width()),
                     NextPowerOfTwo(bitmap_size.height()));
  else
    return gfx::Size(RoundUpMod4(bitmap_size.width()),
                     RoundUpMod4(bitmap_size.height()));
}

template<typename T>
bool ReadBigEndianFromFile(base::File& file, T* out) {
  char buffer[sizeof(T)];
  if (file.ReadAtCurrentPos(buffer, sizeof(T)) != sizeof(T))
    return false;
  base::ReadBigEndian(buffer, out);
  return true;
}

template<typename T>
bool WriteBigEndianToFile(base::File& file, T val) {
  char buffer[sizeof(T)];
  base::WriteBigEndian(buffer, val);
  return file.WriteAtCurrentPos(buffer, sizeof(T)) == sizeof(T);
}

bool ReadBigEndianFloatFromFile(base::File& file, float* out) {
  char buffer[sizeof(float)];
  if (file.ReadAtCurrentPos(buffer, sizeof(buffer)) != sizeof(buffer))
    return false;

#if defined(ARCH_CPU_LITTLE_ENDIAN)
  for (size_t i = 0; i < sizeof(float) / 2; i++) {
    char tmp = buffer[i];
    buffer[i] = buffer[sizeof(float) - 1 - i];
    buffer[sizeof(float) - 1 - i] = tmp;
  }
#endif
  memcpy(out, buffer, sizeof(buffer));

  return true;
}

bool WriteBigEndianFloatToFile(base::File& file, float val) {
  char buffer[sizeof(float)];
  memcpy(buffer, &val, sizeof(buffer));

#if defined(ARCH_CPU_LITTLE_ENDIAN)
  for (size_t i = 0; i < sizeof(float) / 2; i++) {
    char tmp = buffer[i];
    buffer[i] = buffer[sizeof(float) - 1 - i];
    buffer[sizeof(float) - 1 - i] = tmp;
  }
#endif
  return file.WriteAtCurrentPos(buffer, sizeof(buffer)) == sizeof(buffer);
}

}  // anonymous namespace

ThumbnailStore::ThumbnailStore(const std::string& disk_cache_path_str,
                               size_t default_cache_size,
                               size_t approximation_cache_size,
                               size_t compression_queue_max_size,
                               size_t write_queue_max_size,
                               bool use_approximation_thumbnail)
    : disk_cache_path_(disk_cache_path_str),
      compression_queue_max_size_(compression_queue_max_size),
      write_queue_max_size_(write_queue_max_size),
      use_approximation_thumbnail_(use_approximation_thumbnail),
      compression_tasks_count_(0),
      write_tasks_count_(0),
      read_in_progress_(false),
      cache_(default_cache_size),
      approximation_cache_(approximation_cache_size),
      ui_resource_provider_(NULL),
      weak_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

ThumbnailStore::~ThumbnailStore() {
  SetUIResourceProvider(NULL);
}

void ThumbnailStore::SetUIResourceProvider(
    content::UIResourceProvider* ui_resource_provider) {
  if (ui_resource_provider_ == ui_resource_provider)
    return;

  approximation_cache_.Clear();
  cache_.Clear();

  ui_resource_provider_ = ui_resource_provider;
}

void ThumbnailStore::AddThumbnailStoreObserver(
    ThumbnailStoreObserver* observer) {
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void ThumbnailStore::RemoveThumbnailStoreObserver(
    ThumbnailStoreObserver* observer) {
  if (observers_.HasObserver(observer))
    observers_.RemoveObserver(observer);
}

void ThumbnailStore::Put(TabId tab_id,
                         const SkBitmap& bitmap,
                         float thumbnail_scale) {
  if (!ui_resource_provider_ || bitmap.empty() || thumbnail_scale <= 0)
    return;

  DCHECK(thumbnail_meta_data_.find(tab_id) != thumbnail_meta_data_.end());

  base::Time time_stamp = thumbnail_meta_data_[tab_id].capture_time();
  scoped_ptr<Thumbnail> thumbnail = Thumbnail::Create(
      tab_id, time_stamp, thumbnail_scale, ui_resource_provider_, this);
  thumbnail->SetBitmap(bitmap);

  RemoveFromReadQueue(tab_id);
  MakeSpaceForNewItemIfNecessary(tab_id);
  cache_.Put(tab_id, thumbnail.Pass());

  if (use_approximation_thumbnail_) {
    std::pair<SkBitmap, float> approximation =
        CreateApproximation(bitmap, thumbnail_scale);
    scoped_ptr<Thumbnail> approx_thumbnail = Thumbnail::Create(
        tab_id, time_stamp, approximation.second, ui_resource_provider_, this);
    approx_thumbnail->SetBitmap(approximation.first);
    approximation_cache_.Put(tab_id, approx_thumbnail.Pass());
  }
  CompressThumbnailIfNecessary(tab_id, time_stamp, bitmap, thumbnail_scale);
}

void ThumbnailStore::Remove(TabId tab_id) {
  cache_.Remove(tab_id);
  approximation_cache_.Remove(tab_id);
  thumbnail_meta_data_.erase(tab_id);
  RemoveFromDisk(tab_id);
  RemoveFromReadQueue(tab_id);
}

Thumbnail* ThumbnailStore::Get(TabId tab_id, bool force_disk_read) {
  Thumbnail* thumbnail = cache_.Get(tab_id);
  if (thumbnail) {
    thumbnail->CreateUIResource();
    return thumbnail;
  }

  if (force_disk_read &&
      std::find(visible_ids_.begin(), visible_ids_.end(), tab_id) !=
          visible_ids_.end() &&
      std::find(read_queue_.begin(), read_queue_.end(), tab_id) ==
          read_queue_.end()) {
    read_queue_.push_back(tab_id);
    ReadNextThumbnail();
  }

  thumbnail = approximation_cache_.Get(tab_id);
  if (thumbnail) {
    thumbnail->CreateUIResource();
    return thumbnail;
  }

  return NULL;
}

void ThumbnailStore::RemoveFromDiskAtAndAboveId(TabId min_id) {
  base::Closure remove_task =
      base::Bind(&ThumbnailStore::RemoveFromDiskAtAndAboveIdTask,
                 disk_cache_path_,
                 min_id);
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE, remove_task);
}

void ThumbnailStore::InvalidateThumbnailIfChanged(TabId tab_id,
                                                  const GURL& url) {
  ThumbnailMetaDataMap::iterator meta_data_iter =
      thumbnail_meta_data_.find(tab_id);
  if (meta_data_iter == thumbnail_meta_data_.end()) {
    thumbnail_meta_data_[tab_id] = ThumbnailMetaData(base::Time(), url);
  } else if (meta_data_iter->second.url() != url) {
    Remove(tab_id);
  }
}

bool ThumbnailStore::CheckAndUpdateThumbnailMetaData(TabId tab_id,
                                                     const GURL& url) {
  base::Time current_time = base::Time::Now();
  ThumbnailMetaDataMap::iterator meta_data_iter =
      thumbnail_meta_data_.find(tab_id);
  if (meta_data_iter != thumbnail_meta_data_.end() &&
      meta_data_iter->second.url() == url &&
      (current_time - meta_data_iter->second.capture_time()) <
          kCaptureMinRequestTimeMs) {
    return false;
  }

  thumbnail_meta_data_[tab_id] = ThumbnailMetaData(current_time, url);
  return true;
}

void ThumbnailStore::UpdateVisibleIds(const TabIdList& priority) {
  if (priority.empty()) {
    visible_ids_.clear();
    return;
  }

  size_t ids_size = std::min(priority.size(), cache_.MaximumCacheSize());
  if (visible_ids_.size() == ids_size) {
    // Early out if called with the same input as last time (We only care
    // about the first mCache.MaximumCacheSize() entries).
    bool lists_differ = false;
    TabIdList::const_iterator visible_iter = visible_ids_.begin();
    TabIdList::const_iterator priority_iter = priority.begin();
    while (visible_iter != visible_ids_.end() &&
           priority_iter != priority.end()) {
      if (*priority_iter != *visible_iter) {
        lists_differ = true;
        break;
      }
      visible_iter++;
      priority_iter++;
    }

    if (!lists_differ)
      return;
  }

  read_queue_.clear();
  visible_ids_.clear();
  size_t count = 0;
  TabIdList::const_iterator iter = priority.begin();
  while (iter != priority.end() && count < ids_size) {
    TabId tab_id = *iter;
    visible_ids_.push_back(tab_id);
    if (!cache_.Get(tab_id) &&
        std::find(read_queue_.begin(), read_queue_.end(), tab_id) ==
            read_queue_.end()) {
      read_queue_.push_back(tab_id);
    }
    iter++;
    count++;
  }

  ReadNextThumbnail();
}

void ThumbnailStore::DecompressThumbnailFromFile(
    TabId tab_id,
    const base::Callback<void(bool, SkBitmap)>&
        post_decompress_callback) {
  base::FilePath file_path = GetFilePath(tab_id);

  base::Callback<void(skia::RefPtr<SkPixelRef>, float, const gfx::Size&)>
      decompress_task = base::Bind(
          &ThumbnailStore::DecompressionTask, post_decompress_callback);

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ThumbnailStore::ReadTask, true, file_path, decompress_task));
}

void ThumbnailStore::RemoveFromDisk(TabId tab_id) {
  base::FilePath file_path = GetFilePath(tab_id);
  base::Closure task =
      base::Bind(&ThumbnailStore::RemoveFromDiskTask, file_path);
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE, task);
}

void ThumbnailStore::RemoveFromDiskTask(const base::FilePath& file_path) {
  if (base::PathExists(file_path))
    base::DeleteFile(file_path, false);
}

void ThumbnailStore::RemoveFromDiskAtAndAboveIdTask(
    const base::FilePath& dir_path,
    TabId min_id) {
  base::FileEnumerator enumerator(dir_path, false, base::FileEnumerator::FILES);
  while (true) {
    base::FilePath path = enumerator.Next();
    if (path.empty())
      break;
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    TabId tab_id;
    bool success = base::StringToInt(info.GetName().value(), &tab_id);
    if (success && tab_id >= min_id)
      base::DeleteFile(path, false);
  }
}

void ThumbnailStore::WriteThumbnailIfNecessary(
    TabId tab_id,
    skia::RefPtr<SkPixelRef> compressed_data,
    float scale,
    const gfx::Size& content_size) {
  if (write_tasks_count_ >= write_queue_max_size_)
    return;

  write_tasks_count_++;

  base::Callback<void()> post_write_task =
      base::Bind(&ThumbnailStore::PostWriteTask, weak_factory_.GetWeakPtr());
  content::BrowserThread::PostTask(content::BrowserThread::FILE,
                                   FROM_HERE,
                                   base::Bind(&ThumbnailStore::WriteTask,
                                              GetFilePath(tab_id),
                                              compressed_data,
                                              scale,
                                              content_size,
                                              post_write_task));
}

void ThumbnailStore::CompressThumbnailIfNecessary(
    TabId tab_id,
    const base::Time& time_stamp,
    const SkBitmap& bitmap,
    float scale) {
  if (compression_tasks_count_ >= compression_queue_max_size_) {
    RemoveOnMatchedTimeStamp(tab_id, time_stamp);
    return;
  }

  compression_tasks_count_++;

  base::Callback<void(skia::RefPtr<SkPixelRef>, const gfx::Size&)>
      post_compression_task = base::Bind(&ThumbnailStore::PostCompressionTask,
                                         weak_factory_.GetWeakPtr(),
                                         tab_id,
                                         time_stamp,
                                         scale);

  gfx::Size raw_data_size(bitmap.width(), bitmap.height());
  gfx::Size encoded_size = GetEncodedSize(
      raw_data_size, ui_resource_provider_->SupportsETC1NonPowerOfTwo());

  base::WorkerPool::PostTask(FROM_HERE,
                             base::Bind(&ThumbnailStore::CompressionTask,
                                        bitmap,
                                        encoded_size,
                                        post_compression_task),
                             true);
}

void ThumbnailStore::ReadNextThumbnail() {
  if (read_queue_.empty() || read_in_progress_)
    return;

  TabId tab_id = read_queue_.front();
  read_in_progress_ = true;

  base::FilePath file_path = GetFilePath(tab_id);

  base::Callback<void(skia::RefPtr<SkPixelRef>, float, const gfx::Size&)>
      post_read_task = base::Bind(
          &ThumbnailStore::PostReadTask, weak_factory_.GetWeakPtr(), tab_id);

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ThumbnailStore::ReadTask, false, file_path, post_read_task));
}

void ThumbnailStore::MakeSpaceForNewItemIfNecessary(TabId tab_id) {
  if (cache_.Get(tab_id) ||
      std::find(visible_ids_.begin(), visible_ids_.end(), tab_id) ==
          visible_ids_.end() ||
      cache_.size() < cache_.MaximumCacheSize()) {
    return;
  }

  TabId key_to_remove;
  bool found_key_to_remove = false;

  // 1. Find a cached item not in this list
  for (ExpiringThumbnailCache::iterator iter = cache_.begin();
       iter != cache_.end();
       iter++) {
    if (std::find(visible_ids_.begin(), visible_ids_.end(), iter->first) ==
        visible_ids_.end()) {
      key_to_remove = iter->first;
      found_key_to_remove = true;
      break;
    }
  }

  if (!found_key_to_remove) {
    // 2. Find the least important id we can remove.
    for (TabIdList::reverse_iterator riter = visible_ids_.rbegin();
         riter != visible_ids_.rend();
         riter++) {
      if (cache_.Get(*riter)) {
        key_to_remove = *riter;
        break;
        found_key_to_remove = true;
      }
    }
  }

  if (found_key_to_remove)
    cache_.Remove(key_to_remove);
}

void ThumbnailStore::RemoveFromReadQueue(TabId tab_id) {
  TabIdList::iterator read_iter =
      std::find(read_queue_.begin(), read_queue_.end(), tab_id);
  if (read_iter != read_queue_.end())
    read_queue_.erase(read_iter);
}

void ThumbnailStore::InvalidateCachedThumbnail(Thumbnail* thumbnail) {
  DCHECK(thumbnail);
  TabId tab_id = thumbnail->tab_id();
  cc::UIResourceId uid = thumbnail->ui_resource_id();

  Thumbnail* cached_thumbnail = cache_.Get(tab_id);
  if (cached_thumbnail && cached_thumbnail->ui_resource_id() == uid)
    cache_.Remove(tab_id);

  cached_thumbnail = approximation_cache_.Get(tab_id);
  if (cached_thumbnail && cached_thumbnail->ui_resource_id() == uid)
    approximation_cache_.Remove(tab_id);
}

base::FilePath ThumbnailStore::GetFilePath(TabId tab_id) const {
  return disk_cache_path_.Append(base::IntToString(tab_id));
}

namespace {

bool WriteToFile(base::File& file,
                 const gfx::Size& content_size,
                 const float scale,
                 skia::RefPtr<SkPixelRef> compressed_data) {
  if (!file.IsValid())
    return false;

  if (!WriteBigEndianToFile(file, kCompressedKey))
    return false;

  if (!WriteBigEndianToFile(file, content_size.width()))
    return false;

  if (!WriteBigEndianToFile(file, content_size.height()))
    return false;

  // Write ETC1 header.
  compressed_data->lockPixels();

  unsigned char etc1_buffer[ETC_PKM_HEADER_SIZE];
  etc1_pkm_format_header(etc1_buffer,
                         compressed_data->info().width(),
                         compressed_data->info().height());

  int header_bytes_written = file.WriteAtCurrentPos(
      reinterpret_cast<char*>(etc1_buffer), ETC_PKM_HEADER_SIZE);
  if (header_bytes_written != ETC_PKM_HEADER_SIZE)
    return false;

  int data_size = etc1_get_encoded_data_size(
      compressed_data->info().width(),
      compressed_data->info().height());
  int pixel_bytes_written = file.WriteAtCurrentPos(
      reinterpret_cast<char*>(compressed_data->pixels()),
      data_size);
  if (pixel_bytes_written != data_size)
    return false;

  compressed_data->unlockPixels();

  if (!WriteBigEndianToFile(file, kCurrentExtraVersion))
    return false;

  if (!WriteBigEndianFloatToFile(file, 1.f / scale))
    return false;

  return true;
}

}  // anonymous namespace

void ThumbnailStore::WriteTask(const base::FilePath& file_path,
                               skia::RefPtr<SkPixelRef> compressed_data,
                               float scale,
                               const gfx::Size& content_size,
                               const base::Callback<void()>& post_write_task) {
  DCHECK(compressed_data);

  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  bool success = WriteToFile(file,
                             content_size,
                             scale,
                             compressed_data);

  file.Close();

  if (!success)
    base::DeleteFile(file_path, false);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, post_write_task);
}

void ThumbnailStore::PostWriteTask() {
  write_tasks_count_--;
}

void ThumbnailStore::CompressionTask(
    SkBitmap raw_data,
    gfx::Size encoded_size,
    const base::Callback<void(skia::RefPtr<SkPixelRef>, const gfx::Size&)>&
        post_compression_task) {
  skia::RefPtr<SkPixelRef> compressed_data;
  gfx::Size content_size;

  if (!raw_data.empty()) {
    SkAutoLockPixels raw_data_lock(raw_data);
    gfx::Size raw_data_size(raw_data.width(), raw_data.height());
    size_t pixel_size = 4;  // Pixel size is 4 bytes for kARGB_8888_Config.
    size_t stride = pixel_size * raw_data_size.width();

    size_t encoded_bytes =
        etc1_get_encoded_data_size(encoded_size.width(), encoded_size.height());
    SkImageInfo info = SkImageInfo::Make(encoded_size.width(),
                                         encoded_size.height(),
                                         kUnknown_SkColorType,
                                         kUnpremul_SkAlphaType);
    skia::RefPtr<SkData> etc1_pixel_data = skia::AdoptRef(
        SkData::NewUninitialized(encoded_bytes));
    skia::RefPtr<SkMallocPixelRef> etc1_pixel_ref = skia::AdoptRef(
        SkMallocPixelRef::NewWithData(info, 0, NULL, etc1_pixel_data.get()));

    etc1_pixel_ref->lockPixels();
    bool success = etc1_encode_image(
        reinterpret_cast<unsigned char*>(raw_data.getPixels()),
        raw_data_size.width(),
        raw_data_size.height(),
        pixel_size,
        stride,
        reinterpret_cast<unsigned char*>(etc1_pixel_ref->pixels()),
        encoded_size.width(),
        encoded_size.height());
    etc1_pixel_ref->setImmutable();
    etc1_pixel_ref->unlockPixels();

    if (success) {
      compressed_data = etc1_pixel_ref;
      content_size = raw_data_size;
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(post_compression_task, compressed_data, content_size));
}

void ThumbnailStore::PostCompressionTask(
    TabId tab_id,
    const base::Time& time_stamp,
    float scale,
    skia::RefPtr<SkPixelRef> compressed_data,
    const gfx::Size& content_size) {
  compression_tasks_count_--;
  if (!compressed_data) {
    RemoveOnMatchedTimeStamp(tab_id, time_stamp);
    return;
  }

  Thumbnail* thumbnail = cache_.Get(tab_id);
  if (thumbnail) {
    if (thumbnail->time_stamp() != time_stamp)
      return;
    thumbnail->SetCompressedBitmap(compressed_data, content_size);
    thumbnail->CreateUIResource();
  }
  WriteThumbnailIfNecessary(tab_id, compressed_data, scale, content_size);
}

namespace {

bool ReadFromFile(base::File& file,
                  gfx::Size* out_content_size,
                  float* out_scale,
                  skia::RefPtr<SkPixelRef>* out_pixels) {
  if (!file.IsValid())
    return false;

  int key = 0;
  if (!ReadBigEndianFromFile(file, &key))
    return false;

  if (key != kCompressedKey)
    return false;

  int content_width = 0;
  if (!ReadBigEndianFromFile(file, &content_width) || content_width <= 0)
    return false;

  int content_height = 0;
  if (!ReadBigEndianFromFile(file, &content_height) || content_height <= 0)
    return false;

  out_content_size->SetSize(content_width, content_height);

  // Read ETC1 header.
  int header_bytes_read = 0;
  unsigned char etc1_buffer[ETC_PKM_HEADER_SIZE];
  header_bytes_read = file.ReadAtCurrentPos(
      reinterpret_cast<char*>(etc1_buffer),
      ETC_PKM_HEADER_SIZE);
  if (header_bytes_read != ETC_PKM_HEADER_SIZE)
    return false;

  if (!etc1_pkm_is_valid(etc1_buffer))
    return false;

  int raw_width = 0;
  raw_width = etc1_pkm_get_width(etc1_buffer);
  if (raw_width <= 0)
    return false;

  int raw_height = 0;
  raw_height = etc1_pkm_get_height(etc1_buffer);
  if (raw_height <= 0)
    return false;

  // Do some simple sanity check validation.  We can't have thumbnails larger
  // than the max display size of the screen.  We also can't have etc1 texture
  // data larger than the next power of 2 up from that.
  gfx::DeviceDisplayInfo display_info;
  int max_dimension = std::max(display_info.GetDisplayWidth(),
                               display_info.GetDisplayHeight());

  if (content_width > max_dimension
      || content_height > max_dimension
      || static_cast<size_t>(raw_width) > NextPowerOfTwo(max_dimension)
      || static_cast<size_t>(raw_height) > NextPowerOfTwo(max_dimension)) {
    return false;
  }

  int data_size = etc1_get_encoded_data_size(raw_width, raw_height);
  skia::RefPtr<SkData> etc1_pixel_data =
      skia::AdoptRef(SkData::NewUninitialized(data_size));

  int pixel_bytes_read = file.ReadAtCurrentPos(
      reinterpret_cast<char*>(etc1_pixel_data->writable_data()),
      data_size);

  if (pixel_bytes_read != data_size)
    return false;

  SkImageInfo info = SkImageInfo::Make(raw_width,
                                       raw_height,
                                       kUnknown_SkColorType,
                                       kUnpremul_SkAlphaType);

  *out_pixels = skia::AdoptRef(
      SkMallocPixelRef::NewWithData(info,
                                    0,
                                    NULL,
                                    etc1_pixel_data.get()));

  int extra_data_version = 0;
  if (!ReadBigEndianFromFile(file, &extra_data_version))
    return false;

  *out_scale = 1.f;
  if (extra_data_version == 1) {
    if (!ReadBigEndianFloatFromFile(file, out_scale))
      return false;

    if (*out_scale == 0.f)
      return false;

    *out_scale = 1.f / *out_scale;
  }

  return true;
}

}// anonymous namespace

void ThumbnailStore::ReadTask(
    bool decompress,
    const base::FilePath& file_path,
    const base::Callback<
        void(skia::RefPtr<SkPixelRef>, float, const gfx::Size&)>&
        post_read_task) {
  gfx::Size content_size;
  float scale = 0.f;
  skia::RefPtr<SkPixelRef> compressed_data;

  if (base::PathExists(file_path)) {
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);


    bool valid_contents = ReadFromFile(file,
                                       &content_size,
                                       &scale,
                                       &compressed_data);
    file.Close();

    if (!valid_contents) {
      content_size.SetSize(0, 0);
            scale = 0.f;
            compressed_data.clear();
            base::DeleteFile(file_path, false);
    }
  }

  if (decompress) {
    base::WorkerPool::PostTask(
        FROM_HERE,
        base::Bind(post_read_task, compressed_data, scale, content_size),
        true);
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(post_read_task, compressed_data, scale, content_size));
  }
}

void ThumbnailStore::PostReadTask(TabId tab_id,
                                  skia::RefPtr<SkPixelRef> compressed_data,
                                  float scale,
                                  const gfx::Size& content_size) {
  read_in_progress_ = false;

  TabIdList::iterator iter =
      std::find(read_queue_.begin(), read_queue_.end(), tab_id);
  if (iter == read_queue_.end()) {
    ReadNextThumbnail();
    return;
  }

  read_queue_.erase(iter);

  if (!cache_.Get(tab_id) && compressed_data) {
    ThumbnailMetaDataMap::iterator meta_iter =
        thumbnail_meta_data_.find(tab_id);
    base::Time time_stamp = base::Time::Now();
    if (meta_iter != thumbnail_meta_data_.end())
      time_stamp = meta_iter->second.capture_time();

    MakeSpaceForNewItemIfNecessary(tab_id);
    scoped_ptr<Thumbnail> thumbnail = Thumbnail::Create(
        tab_id, time_stamp, scale, ui_resource_provider_, this);
    thumbnail->SetCompressedBitmap(compressed_data,
                                   content_size);
    if (kPreferCPUMemory)
      thumbnail->CreateUIResource();

    cache_.Put(tab_id, thumbnail.Pass());
    NotifyObserversOfThumbnailRead(tab_id);
  }

  ReadNextThumbnail();
}

void ThumbnailStore::NotifyObserversOfThumbnailRead(TabId tab_id) {
  FOR_EACH_OBSERVER(
      ThumbnailStoreObserver, observers_, OnFinishedThumbnailRead(tab_id));
}

void ThumbnailStore::RemoveOnMatchedTimeStamp(TabId tab_id,
                                              const base::Time& time_stamp) {
  // We remove the cached version if it matches the tab_id and the time_stamp.
  Thumbnail* thumbnail = cache_.Get(tab_id);
  Thumbnail* approx_thumbnail = approximation_cache_.Get(tab_id);
  if ((thumbnail && thumbnail->time_stamp() == time_stamp) ||
      (approx_thumbnail && approx_thumbnail->time_stamp() == time_stamp)) {
    Remove(tab_id);
  }
  return;
}

void ThumbnailStore::DecompressionTask(
    const base::Callback<void(bool, SkBitmap)>&
        post_decompression_callback,
    skia::RefPtr<SkPixelRef> compressed_data,
    float scale,
    const gfx::Size& content_size) {
  SkBitmap raw_data_small;
  bool success = false;

  if (compressed_data.get()) {
    gfx::Size buffer_size = gfx::Size(compressed_data->info().width(),
                                      compressed_data->info().height());

    SkBitmap raw_data;
    raw_data.allocPixels(SkImageInfo::Make(buffer_size.width(),
                                           buffer_size.height(),
                                           kRGBA_8888_SkColorType,
                                           kOpaque_SkAlphaType));
    SkAutoLockPixels raw_data_lock(raw_data);
    compressed_data->lockPixels();
    success = etc1_decode_image(
        reinterpret_cast<unsigned char*>(compressed_data->pixels()),
        reinterpret_cast<unsigned char*>(raw_data.getPixels()),
        buffer_size.width(),
        buffer_size.height(),
        raw_data.bytesPerPixel(),
        raw_data.rowBytes());
    compressed_data->unlockPixels();
    raw_data.setImmutable();

    if (!success) {
      // Leave raw_data_small empty for consistency with other failure modes.
    } else if (content_size == buffer_size) {
      // Shallow copy the pixel reference.
      raw_data_small = raw_data;
    } else {
      // The content size is smaller than the buffer size (likely because of
      // a power-of-two rounding), so deep copy the bitmap.
      raw_data_small.allocPixels(SkImageInfo::Make(content_size.width(),
                                                   content_size.height(),
                                                   kRGBA_8888_SkColorType,
                                                   kOpaque_SkAlphaType));
      SkAutoLockPixels raw_data_small_lock(raw_data_small);
      SkCanvas small_canvas(raw_data_small);
      small_canvas.drawBitmap(raw_data, 0, 0);
      raw_data_small.setImmutable();
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(post_decompression_callback, success, raw_data_small));
}

ThumbnailStore::ThumbnailMetaData::ThumbnailMetaData() {
}

ThumbnailStore::ThumbnailMetaData::ThumbnailMetaData(
    const base::Time& current_time,
    const GURL& url)
    : capture_time_(current_time), url_(url) {
}

std::pair<SkBitmap, float> ThumbnailStore::CreateApproximation(
    const SkBitmap& bitmap,
    float scale) {
  DCHECK(!bitmap.empty());
  DCHECK_GT(scale, 0);
  SkAutoLockPixels bitmap_lock(bitmap);
  float new_scale = 1.f / kApproximationScaleFactor;

  gfx::Size dst_size = gfx::ToFlooredSize(
      gfx::ScaleSize(gfx::Size(bitmap.width(), bitmap.height()), new_scale));
  SkBitmap dst_bitmap;
  dst_bitmap.allocPixels(SkImageInfo::Make(dst_size.width(),
                                           dst_size.height(),
                                           bitmap.info().fColorType,
                                           bitmap.info().fAlphaType));
  dst_bitmap.eraseColor(0);
  SkAutoLockPixels dst_bitmap_lock(dst_bitmap);

  SkCanvas canvas(dst_bitmap);
  canvas.scale(new_scale, new_scale);
  canvas.drawBitmap(bitmap, 0, 0, NULL);
  dst_bitmap.setImmutable();

  return std::make_pair(dst_bitmap, new_scale * scale);
}
