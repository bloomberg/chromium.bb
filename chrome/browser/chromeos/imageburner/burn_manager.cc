// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/imageburner/burn_manager.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/download/download_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/file_stream.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;
using content::DownloadManager;
using content::WebContents;

namespace chromeos {
namespace imageburner {

namespace {

const char kConfigFileUrl[] =
    "https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf";
const char kTempImageFolderName[] = "chromeos_image";

BurnManager* g_burn_manager = NULL;

// Cretes a directory and calls |callback| with the result on UI thread.
void CreateDirectory(const FilePath& path,
                     base::Callback<void(bool success)> callback) {
  const bool success = file_util::CreateDirectory(path);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, success));
}

// Creates a FileStream and calls |callback| with the result on UI thread.
void CreateFileStream(
    const FilePath& file_path,
    base::Callback<void(net::FileStream* file_stream)> callback) {
  scoped_ptr<net::FileStream> file_stream(new net::FileStream(NULL));
  // TODO(tbarzic): Save temp image file to temp folder instead of Downloads
  // once extracting image directly to removalbe device is implemented
  if (file_stream->OpenSync(file_path, base::PLATFORM_FILE_OPEN_ALWAYS |
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
      config_file_url_(kConfigFileUrl),
      state_machine_(new StateMachine()),
      downloader_(NULL) {
}

BurnManager::~BurnManager() {
  if (!image_dir_.empty()) {
    file_util::Delete(image_dir_, true);
  }
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

void BurnManager::FetchConfigFile(Delegate* delegate) {
  if (config_file_fetched()) {
    delegate->OnConfigFileFetched(config_file_, true);
    return;
  }
  downloaders_.push_back(delegate->AsWeakPtr());

  if (config_fetcher_.get())
    return;

  config_fetcher_.reset(content::URLFetcher::Create(
      config_file_url_, content::URLFetcher::GET, this));
  config_fetcher_->StartWithRequestContextGetter(
      g_browser_process->system_request_context());
}

void BurnManager::OnURLFetchComplete(const content::URLFetcher* source) {
  if (source == config_fetcher_.get()) {
    std::string data;
    const bool success =
        source->GetStatus().status() == net::URLRequestStatus::SUCCESS;
    if (success)
      config_fetcher_->GetResponseAsString(&data);
    config_fetcher_.reset();
    ConfigFileFetched(success, data);
  }
}

void BurnManager::ConfigFileFetched(bool fetched, const std::string& content) {
  if (config_file_fetched())
    return;

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

    download_util::RecordDownloadSource(
        download_util::INITIATED_BY_IMAGE_BURNER);
    download_manager->DownloadUrl(
        url,
        web_contents->GetURL(),
        web_contents->GetEncoding(),
        false,
        -1,
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
