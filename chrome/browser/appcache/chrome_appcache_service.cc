// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/appcache/chrome_appcache_service.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/cookie_prompt_modal_dialog_delegate.h"
#include "chrome/browser/message_box_handler.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "net/base/net_errors.h"
#include "webkit/appcache/appcache_thread.h"

static bool has_initialized_thread_ids;

// ChromeAppCacheService cannot just subclass the delegate interface
// because we may have several prompts pending.
class ChromeAppCacheService::PromptDelegate
    : public CookiePromptModalDialogDelegate {
 public:
  PromptDelegate(ChromeAppCacheService* service,
                 const GURL& manifest_url, net::CompletionCallback* callback)
      : service_(service), manifest_url_(manifest_url), callback_(callback) {
  }

  virtual void AllowSiteData(bool session_expire) {
    service_->DidPrompt(net::OK, manifest_url_, callback_);
    delete this;
  }

  virtual void BlockSiteData() {
    service_->DidPrompt(net::ERR_ACCESS_DENIED,  manifest_url_, callback_);
    delete this;
  }

 private:
  scoped_refptr<ChromeAppCacheService> service_;
  GURL manifest_url_;
  net::CompletionCallback* callback_;
};

// ----------------------------------------------------------------------------

ChromeAppCacheService::ChromeAppCacheService(
    const FilePath& profile_path,
    ChromeURLRequestContext* request_context) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(request_context);

  if (!has_initialized_thread_ids) {
    has_initialized_thread_ids = true;
    appcache::AppCacheThread::Init(ChromeThread::DB, ChromeThread::IO);
  }

  host_contents_settings_map_ = request_context->host_content_settings_map();
  registrar_.Add(
      this, NotificationType::PURGE_MEMORY, NotificationService::AllSources());

  // Init our base class.
  Initialize(request_context->is_off_the_record() ?
                 FilePath() : profile_path.Append(chrome::kAppCacheDirname),
             ChromeThread::GetMessageLoopProxyForThread(ChromeThread::CACHE));
  set_request_context(request_context);
  set_appcache_policy(this);
}

ChromeAppCacheService::~ChromeAppCacheService() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
}

// static
void ChromeAppCacheService::ClearLocalState(const FilePath& profile_path) {
  file_util::Delete(profile_path.Append(chrome::kAppCacheDirname), true);
}

bool ChromeAppCacheService::CanLoadAppCache(const GURL& manifest_url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  ContentSetting setting = host_contents_settings_map_->GetContentSetting(
      manifest_url, CONTENT_SETTINGS_TYPE_COOKIES);
  DCHECK(setting != CONTENT_SETTING_DEFAULT);
  // We don't prompt for read access.
  return setting != CONTENT_SETTING_BLOCK;
}

int ChromeAppCacheService::CanCreateAppCache(
    const GURL& manifest_url, net::CompletionCallback* callback) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  ContentSetting setting = host_contents_settings_map_->GetContentSetting(
      manifest_url, CONTENT_SETTINGS_TYPE_COOKIES);
  DCHECK(setting != CONTENT_SETTING_DEFAULT);
  if (setting == CONTENT_SETTING_ASK) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ChromeAppCacheService::DoPrompt,
                          manifest_url, callback));
    return net::ERR_IO_PENDING;
  }
  return (setting != CONTENT_SETTING_BLOCK) ? net::OK :
                                              net::ERR_ACCESS_DENIED;
}

void ChromeAppCacheService::DoPrompt(
    const GURL& manifest_url, net::CompletionCallback* callback) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // The setting may have changed (due to the "remember" option)
  ContentSetting setting = host_contents_settings_map_->GetContentSetting(
      manifest_url, CONTENT_SETTINGS_TYPE_COOKIES);
  if (setting != CONTENT_SETTING_ASK) {
    int rv = (setting != CONTENT_SETTING_BLOCK) ? net::OK :
                                                  net::ERR_ACCESS_DENIED;
    DidPrompt(rv, manifest_url, callback);
    return;
  }

  // Show the prompt on top of the current tab.
  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !browser->GetSelectedTabContents()) {
    DidPrompt(net::ERR_ACCESS_DENIED, manifest_url, callback);
    return;
  }

  RunAppCachePrompt(browser->GetSelectedTabContents(),
                    host_contents_settings_map_, manifest_url,
                    new PromptDelegate(this, manifest_url, callback));
}

void ChromeAppCacheService::DidPrompt(
    int rv, const GURL& manifest_url, net::CompletionCallback* callback) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ChromeAppCacheService::CallCallback,
                        rv, callback));
}

void ChromeAppCacheService::CallCallback(
    int rv, net::CompletionCallback* callback) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  callback->Run(rv);
}

void ChromeAppCacheService::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(type == NotificationType::PURGE_MEMORY);
  PurgeMemory();
}

// ----------------------------------------------------------------------------

static ChromeThread::ID ToChromeThreadID(int id) {
  DCHECK(has_initialized_thread_ids);
  DCHECK(id == ChromeThread::DB || id == ChromeThread::IO);
  return static_cast<ChromeThread::ID>(id);
}

namespace appcache {

// An impl of AppCacheThread we need to provide to the appcache lib.

bool AppCacheThread::PostTask(
    int id,
    const tracked_objects::Location& from_here,
    Task* task) {
  return ChromeThread::PostTask(ToChromeThreadID(id), from_here, task);
}

bool AppCacheThread::CurrentlyOn(int id) {
  return ChromeThread::CurrentlyOn(ToChromeThreadID(id));
}

}  // namespace appcache
