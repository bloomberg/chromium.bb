// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/ui/thumbnail_provider.h"

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/image_decoder.h"
#include "content/public/browser/browser_thread.h"
#include "jni/ThumbnailProviderImpl_jni.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/skbitmap_operations.h"

class SkBitmap;

using base::android::JavaParamRef;

namespace {

// Ignore image files that are too large to avoid long delays.
const int64_t kMaxImageSize = 10 * 1024 * 1024;  // 10 MB

class ImageThumbnailRequest : public ImageDecoder::ImageRequest {
 public:
  static void Create(int icon_size,
                     const std::string& file_path,
                     base::WeakPtr<ThumbnailProvider> weak_provider) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
    ImageThumbnailRequest* request =
        new ImageThumbnailRequest(icon_size, file_path, weak_provider);
    request->Start();
  }

 protected:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

    SkBitmap thumbnail = decoded_image;
    if (!decoded_image.drawsNothing()) {
      // Shrink the image down so that its smallest dimension is equal to or
      // smaller than the requested size.
      int min_dimension =
          std::min(decoded_image.width(), decoded_image.height());

      if (min_dimension > icon_size_) {
        uint64_t width = static_cast<uint64_t>(decoded_image.width());
        uint64_t height = static_cast<uint64_t>(decoded_image.height());
        thumbnail = skia::ImageOperations::Resize(
            decoded_image, skia::ImageOperations::RESIZE_BEST,
            width * icon_size_ / min_dimension,
            height * icon_size_ / min_dimension);
      }
    }

    FinishRequest(thumbnail);
  }

  void OnDecodeImageFailed() override {
    LOG(ERROR) << "Failed to decode image: " << file_path_;
    FinishRequest(SkBitmap());
  }

 private:
  ImageThumbnailRequest(int icon_size,
                        const std::string& file_path,
                        base::WeakPtr<ThumbnailProvider> weak_provider)
      : icon_size_(icon_size),
        file_path_(file_path),
        weak_provider_(weak_provider) {}

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

    // Confirm that the file's size is within our threshold.
    int64_t file_size;
    base::FilePath path = base::FilePath::FromUTF8Unsafe(file_path_);
    if (!base::GetFileSize(path, &file_size) || file_size > kMaxImageSize) {
      LOG(ERROR) << "Unexpected file size: " << file_path_ << ", " << file_size;
      FinishRequest(SkBitmap());
      return;
    }

    // Make sure the file isn't empty.
    std::string data;
    bool success = base::ReadFileToString(path, &data);
    if (!success || data.empty()) {
      LOG(ERROR) << "Failed to read file: " << file_path_;
      FinishRequest(SkBitmap());
      return;
    }

    ImageDecoder::Start(this, data);
  }

  void FinishRequest(const SkBitmap& thumbnail) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ThumbnailProvider::OnThumbnailRetrieved,
                   weak_provider_, file_path_, thumbnail));
    task_runner()->DeleteSoon(FROM_HERE, this);
  }

  const int icon_size_;
  std::string file_path_;
  base::WeakPtr<ThumbnailProvider> weak_provider_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ImageThumbnailRequest);
};

}  // namespace

ThumbnailProvider::ThumbnailProvider(const JavaParamRef<jobject>& jobj)
    : java_delegate_(jobj), weak_factory_(this) {}

ThumbnailProvider::~ThumbnailProvider() {
  java_delegate_.Reset();
}

void ThumbnailProvider::Destroy(JNIEnv* env,
                                const JavaParamRef<jobject>& jobj) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delete this;
}

void ThumbnailProvider::OnThumbnailRetrieved(const std::string& file_path,
                                             const SkBitmap& thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (java_delegate_.is_null())
    return;

  // Send the bitmap back to Java-land.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ThumbnailProviderImpl_onThumbnailRetrieved(
      env, java_delegate_,
      base::android::ConvertUTF8ToJavaString(env, file_path),
      thumbnail.drawsNothing() ? NULL
          : gfx::ConvertToJavaBitmap(&thumbnail));
}

void ThumbnailProvider::RetrieveThumbnail(JNIEnv* env,
                                          const JavaParamRef<jobject>& jobj,
                                          jstring jfile_path,
                                          jint icon_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The ImageThumbnailRequest deletes itself on completion.  Don't track it
  // because we don't (currently) cancel it.
  std::string path = base::android::ConvertJavaStringToUTF8(env, jfile_path);
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&ImageThumbnailRequest::Create, icon_size, path,
                 weak_factory_.GetWeakPtr()));
}

// static
bool ThumbnailProvider::RegisterThumbnailProvider(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  return reinterpret_cast<intptr_t>(new ThumbnailProvider(jobj));
}
