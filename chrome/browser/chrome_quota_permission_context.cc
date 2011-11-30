// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_quota_permission_context.h"

#include <string>

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/quota/quota_types.h"

using content::BrowserThread;

namespace {

// If we requested larger quota than this threshold, show a different
// message to the user.
const int64 kRequestLargeQuotaThreshold = 5 * 1024 * 1024;

class RequestQuotaInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  typedef QuotaPermissionContext::PermissionCallback PermissionCallback;

  RequestQuotaInfoBarDelegate(
      InfoBarTabHelper* infobar_helper,
      ChromeQuotaPermissionContext* context,
      const GURL& origin_url,
      int64 requested_quota,
      const std::string& display_languages,
      const PermissionCallback& callback)
      : ConfirmInfoBarDelegate(infobar_helper),
        context_(context),
        origin_url_(origin_url),
        display_languages_(display_languages),
        requested_quota_(requested_quota),
        callback_(callback) {}

 private:
  virtual ~RequestQuotaInfoBarDelegate() {
    if (!callback_.is_null())
      context_->DispatchCallbackOnIOThread(
          callback_, QuotaPermissionContext::kResponseCancelled);
  }

  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details)
      const OVERRIDE {
    return false;
  }

  virtual string16 GetMessageText() const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  scoped_refptr<ChromeQuotaPermissionContext> context_;
  GURL origin_url_;
  std::string display_languages_;
  int64 requested_quota_;
  PermissionCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(RequestQuotaInfoBarDelegate);
};

void RequestQuotaInfoBarDelegate::InfoBarDismissed() {
  context_->DispatchCallbackOnIOThread(
      callback_, QuotaPermissionContext::kResponseCancelled);
}

string16 RequestQuotaInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      (requested_quota_ > kRequestLargeQuotaThreshold ?
          IDS_REQUEST_LARGE_QUOTA_INFOBAR_QUESTION :
          IDS_REQUEST_QUOTA_INFOBAR_QUESTION),
      net::FormatUrl(origin_url_, display_languages_));
}

bool RequestQuotaInfoBarDelegate::Accept() {
  context_->DispatchCallbackOnIOThread(
      callback_, QuotaPermissionContext::kResponseAllow);
  return true;
}

bool RequestQuotaInfoBarDelegate::Cancel() {
  context_->DispatchCallbackOnIOThread(
      callback_, QuotaPermissionContext::kResponseCancelled);
  return true;
}

}  // anonymous namespace

ChromeQuotaPermissionContext::ChromeQuotaPermissionContext() {
}

ChromeQuotaPermissionContext::~ChromeQuotaPermissionContext() {
}

void ChromeQuotaPermissionContext::RequestQuotaPermission(
    const GURL& origin_url,
    quota::StorageType type,
    int64 requested_quota,
    int render_process_id,
    int render_view_id,
    const PermissionCallback& callback) {
  if (type != quota::kStorageTypePersistent) {
    // For now we only support requesting quota with this interface
    // for Persistent storage type.
    callback.Run(kResponseDisallow);
    return;
  }

  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeQuotaPermissionContext::RequestQuotaPermission, this,
                   origin_url, type, requested_quota, render_process_id,
                   render_view_id, callback));
    return;
  }

  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);
  if (!tab_contents) {
    // The tab may have gone away or the request may not be from a tab.
    LOG(WARNING) << "Attempt to request quota tabless renderer: "
                 << render_process_id << "," << render_view_id;
    DispatchCallbackOnIOThread(callback, kResponseCancelled);
    return;
  }

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  InfoBarTabHelper* infobar_helper = wrapper->infobar_tab_helper();
  infobar_helper->AddInfoBar(new RequestQuotaInfoBarDelegate(
      infobar_helper, this, origin_url, requested_quota,
      wrapper->profile()->GetPrefs()->GetString(prefs::kAcceptLanguages),
      callback));
}

void ChromeQuotaPermissionContext::DispatchCallbackOnIOThread(
    const PermissionCallback& callback,
    Response response) {
  DCHECK_EQ(false, callback.is_null());

  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeQuotaPermissionContext::DispatchCallbackOnIOThread,
                   this, callback, response));
    return;
  }

  callback.Run(response);
}
