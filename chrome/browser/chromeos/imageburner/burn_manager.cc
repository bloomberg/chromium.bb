// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/imageburner/burn_manager.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/download/download_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
using content::WebContents;

namespace chromeos {
namespace imageburner {

namespace {

const char kConfigFileUrl[] =
    "https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf";
const char kTempImageFolderName[] = "chromeos_image";
const char kConfigFileName[] = "recovery.conf";

BurnManager* g_burn_manager = NULL;

// Cretes a directory and calls |callback| with the result on UI thread.
void CreateDirectory(const FilePath& path,
                     base::Callback<void(bool success)> callback) {
  const bool success = file_util::CreateDirectory(path);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, success));
}

// Reads file content and calls |callback| with the result on UI thread.
void ReadFile(const FilePath& path,
              base::Callback<void(bool success,
                                  const std::string& file_content)> callback) {
  std::string file_content;
  const bool success = file_util::ReadFileToString(path, &file_content);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, success, file_content));
}

// Creates a FileStream and calls |callback| with the result on UI thread.
void CreateFileStream(
    const FilePath& file_path,
    base::Callback<void(net::FileStream* file_stream)> callback) {
  scoped_ptr<net::FileStream> file_stream(new net::FileStream(NULL));
  // TODO(tbarzic): Save temp image file to temp folder instead of Downloads
  // once extracting image directly to removalbe device is implemented
  if (file_stream->Open(file_path, base::PLATFORM_FILE_OPEN_ALWAYS |
                        base::PLATFORM_FILE_WRITE))
    file_stream.reset();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, file_stream.release()));
}

}  // namespace

const char kName[] = "name";
const char kHwid[] = "hwid";
const char kFileName[] = "file";
const char kUrl[] = "url";

////////////////////////////////////////////////////////////////////////////////
//
// ConfigFile
//
////////////////////////////////////////////////////////////////////////////////
ConfigFile::ConfigFile() {
}

ConfigFile::ConfigFile(const std::string& file_content) {
  reset(file_content);
}

ConfigFile::~ConfigFile() {
}

void ConfigFile::reset(const std::string& file_content) {
  clear();

  std::vector<std::string> lines;
  Tokenize(file_content, "\n", &lines);

  std::vector<std::string> key_value_pair;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].empty())
      continue;

    key_value_pair.clear();
    Tokenize(lines[i], "=", &key_value_pair);
    // Skip lines that don't contain key-value pair and lines without a key.
    if (key_value_pair.size() != 2 || key_value_pair[0].empty())
      continue;

    ProcessLine(key_value_pair);
  }

  // Make sure last block has at least one hwid associated with it.
  DeleteLastBlockIfHasNoHwid();
}

void ConfigFile::clear() {
  config_struct_.clear();
}

const std::string& ConfigFile::GetProperty(
    const std::string& property_name,
    const std::string& hwid) const {
  // We search for block that has desired hwid property, and if we find it, we
  // return its property_name property.
  for (BlockList::const_iterator block_it = config_struct_.begin();
       block_it != config_struct_.end();
       ++block_it) {
    if (block_it->hwids.find(hwid) != block_it->hwids.end()) {
      PropertyMap::const_iterator property =
          block_it->properties.find(property_name);
      if (property != block_it->properties.end()) {
        return property->second;
      } else {
        return EmptyString();
      }
    }
  }

  return EmptyString();
}

// Check if last block has a hwid associated with it, and erase it if it
// doesn't,
void ConfigFile::DeleteLastBlockIfHasNoHwid() {
  if (!config_struct_.empty() && config_struct_.back().hwids.empty()) {
    config_struct_.pop_back();
  }
}

void ConfigFile::ProcessLine(const std::vector<std::string>& line) {
  // If line contains name key, new image block is starting, so we have to add
  // new entry to our data structure.
  if (line[0] == kName) {
    // If there was no hardware class defined for previous block, we can
    // disregard is since we won't be abble to access any of its properties
    // anyway. This should not happen, but let's be defensive.
    DeleteLastBlockIfHasNoHwid();
    config_struct_.resize(config_struct_.size() + 1);
  }

  // If we still haven't added any blocks to data struct, we disregard this
  // line. Again, this should never happen.
  if (config_struct_.empty())
    return;

  ConfigFileBlock& last_block = config_struct_.back();

  if (line[0] == kHwid) {
    // Check if line contains hwid property. If so, add it to set of hwids
    // associated with current block.
    last_block.hwids.insert(line[1]);
  } else {
    // Add new block property.
    last_block.properties.insert(std::make_pair(line[0], line[1]));
  }
}

ConfigFile::ConfigFileBlock::ConfigFileBlock() {
}

ConfigFile::ConfigFileBlock::~ConfigFileBlock() {
}

////////////////////////////////////////////////////////////////////////////////
//
// StateMachine
//
////////////////////////////////////////////////////////////////////////////////
StateMachine::StateMachine()
    : image_download_requested_(false),
      download_started_(false),
      download_finished_(false),
      state_(INITIAL) {
}

StateMachine::~StateMachine() {
}

void StateMachine::OnError(int error_message_id) {
  if (state_ == INITIAL)
    return;
  if (!download_finished_) {
    download_started_ = false;
    image_download_requested_ = false;
  }
  state_ = INITIAL;
  FOR_EACH_OBSERVER(Observer, observers_, OnError(error_message_id));
}

void StateMachine::OnSuccess() {
  if (state_ == INITIAL)
    return;
  state_ = INITIAL;
  OnStateChanged();
}

void StateMachine::OnCancelation() {
  // We use state CANCELLED only to let observers know that they have to
  // process cancelation. We don't actually change the state.
  FOR_EACH_OBSERVER(Observer, observers_, OnBurnStateChanged(CANCELLED));
}

////////////////////////////////////////////////////////////////////////////////
//
// BurnManager
//
////////////////////////////////////////////////////////////////////////////////

BurnManager::BurnManager()
    : weak_ptr_factory_(this),
      download_manager_(NULL),
      download_item_observer_added_(false),
      active_download_item_(NULL),
      config_file_url_(kConfigFileUrl),
      config_file_requested_(false),
      config_file_fetched_(false),
      state_machine_(new StateMachine()),
      downloader_(NULL) {
}

BurnManager::~BurnManager() {
  if (!image_dir_.empty()) {
    file_util::Delete(image_dir_, true);
  }
  if (active_download_item_)
    active_download_item_->RemoveObserver(this);
  if (download_manager_)
    download_manager_->RemoveObserver(this);
}

// static
void BurnManager::Initialize() {
  if (g_burn_manager) {
    LOG(WARNING) << "BurnManager was already initialized";
    return;
  }
  g_burn_manager = new BurnManager();
  VLOG(1) << "BurnManager initialized";
}

// static
void BurnManager::Shutdown() {
  if (!g_burn_manager) {
    LOG(WARNING) << "BurnManager::Shutdown() called with NULL manager";
    return;
  }
  delete g_burn_manager;
  g_burn_manager = NULL;
  VLOG(1) << "BurnManager Shutdown completed";
}

// static
BurnManager* BurnManager::GetInstance() {
  return g_burn_manager;
}

void BurnManager::OnDownloadUpdated(DownloadItem* download) {
  if (download->IsCancelled()) {
    ConfigFileFetched(false, "");
    DCHECK(!download_item_observer_added_);
    DCHECK(active_download_item_ == NULL);
  } else if (download->IsComplete()) {
    OnConfigFileDownloaded();
  }
}

void BurnManager::OnConfigFileDownloaded() {
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(ReadFile,
                 config_file_path_,
                 base::Bind(&BurnManager::ConfigFileFetched,
                            weak_ptr_factory_.GetWeakPtr())));
}

void BurnManager::ModelChanged() {
  std::vector<DownloadItem*> downloads;
  download_manager_->GetTemporaryDownloads(GetImageDir(), &downloads);
  if (download_item_observer_added_)
    return;
  for (std::vector<DownloadItem*>::const_iterator it = downloads.begin();
      it != downloads.end();
      ++it) {
    if ((*it)->GetURL() == config_file_url_) {
      download_item_observer_added_ = true;
      (*it)->AddObserver(this);
      active_download_item_ = *it;
      break;
    }
  }
}

void BurnManager::OnBurnDownloadStarted(bool success) {
  if (!success)
    ConfigFileFetched(false, "");
}

void BurnManager::CreateImageDir(Delegate* delegate) {
  if (image_dir_.empty()) {
    CHECK(PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &image_dir_));
    image_dir_ = image_dir_.Append(kTempImageFolderName);
    BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(CreateDirectory,
                   image_dir_,
                   base::Bind(&BurnManager::OnImageDirCreated,
                              weak_ptr_factory_.GetWeakPtr(),
                              delegate)));
  } else {
    const bool success = true;
    OnImageDirCreated(delegate, success);
  }
}

void BurnManager::OnImageDirCreated(Delegate* delegate, bool success) {
  delegate->OnImageDirCreated(success);
}

const FilePath& BurnManager::GetImageDir() {
  return image_dir_;
}

void BurnManager::FetchConfigFile(WebContents* web_contents,
                                  Delegate* delegate) {
  if (config_file_fetched_) {
    delegate->OnConfigFileFetched(config_file_, true);
    return;
  }
  downloaders_.push_back(delegate->AsWeakPtr());

  if (config_file_requested_)
    return;
  config_file_requested_ = true;

  config_file_path_ = GetImageDir().Append(kConfigFileName);
  download_manager_ = web_contents->GetBrowserContext()->GetDownloadManager();
  download_manager_->AddObserver(this);
  downloader()->AddListener(this, config_file_url_);
  downloader()->DownloadFile(config_file_url_, config_file_path_, web_contents);
}

void BurnManager::ConfigFileFetched(bool fetched, const std::string& content) {
  if (config_file_fetched_)
    return;

  if (active_download_item_) {
    active_download_item_->RemoveObserver(this);
    active_download_item_ = NULL;
  }
  download_item_observer_added_ = false;
  if (download_manager_)
    download_manager_->RemoveObserver(this);

  config_file_fetched_ = fetched;

  if (fetched) {
    config_file_.reset(content);
  } else {
    config_file_.clear();
  }

  for (size_t i = 0; i < downloaders_.size(); ++i) {
    if (downloaders_[i]) {
      downloaders_[i]->OnConfigFileFetched(config_file_, fetched);
    }
  }
  downloaders_.clear();
}

////////////////////////////////////////////////////////////////////////////////
//
// Downloader
//
////////////////////////////////////////////////////////////////////////////////

Downloader::Downloader() : weak_ptr_factory_(this) {}

Downloader::~Downloader() {}

void Downloader::DownloadFile(const GURL& url,
    const FilePath& file_path, WebContents* web_contents) {
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(CreateFileStream,
                 file_path,
                 base::Bind(&Downloader::OnFileStreamCreated,
                            weak_ptr_factory_.GetWeakPtr(),
                            url,
                            file_path,
                            web_contents)));
}

void Downloader::OnFileStreamCreated(const GURL& url,
                                     const FilePath& file_path,
                                     WebContents* web_contents,
                                     net::FileStream* file_stream) {
  if (file_stream) {
    DownloadManager* download_manager =
        web_contents->GetBrowserContext()->GetDownloadManager();
    DownloadSaveInfo save_info;
    save_info.file_path = file_path;
    save_info.file_stream = linked_ptr<net::FileStream>(file_stream);
    DownloadStarted(true, url);

    download_util::RecordDownloadCount(
        download_util::INITIATED_BY_IMAGE_BURNER_COUNT);
    download_manager->DownloadUrl(
        url,
        web_contents->GetURL(),
        web_contents->GetEncoding(),
        false,
        save_info,
        web_contents);
  } else {
    DownloadStarted(false, url);
  }
}

void Downloader::AddListener(Listener* listener, const GURL& url) {
  listeners_.insert(std::make_pair(url, listener->AsWeakPtr()));
}

void Downloader::DownloadStarted(bool success, const GURL& url) {
  std::pair<ListenerMap::iterator, ListenerMap::iterator> listener_range =
      listeners_.equal_range(url);
  for (ListenerMap::iterator current_listener = listener_range.first;
       current_listener != listener_range.second;
       ++current_listener) {
    if (current_listener->second)
      current_listener->second->OnBurnDownloadStarted(success);
  }
  listeners_.erase(listener_range.first, listener_range.second);
}

}  // namespace imageburner
}  // namespace chromeos
