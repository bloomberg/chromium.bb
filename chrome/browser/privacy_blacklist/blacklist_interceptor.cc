// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_interceptor.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_manager.h"
#include "chrome/browser/privacy_blacklist/blacklist_request_info.h"
#include "chrome/common/jstemplate_builder.h"
#include "net/url_request/url_request_simple_job.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

class URLRequestBlacklistJob : public URLRequestSimpleJob {
 public:
  URLRequestBlacklistJob(URLRequest* request,
                         const BlacklistRequestInfo& request_info)
      : URLRequestSimpleJob(request),
        request_info_(request_info) {
  }

  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const {
    if (ResourceType::IsFrame(request_info_.resource_type())) {
      *mime_type = "text/html";
      *charset = "utf-8";
      *data = GetHTMLResponse();
    } else {
      // TODO(phajdan.jr): For some resources (like script) we may want to
      // simulate an error instead or return other MIME type.
      *mime_type = "image/png";
      *data = GetImageResponse();
    }
    return true;
  }

 private:
  std::string GetHTMLResponse() const {
      return "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/html;charset=utf-8\r\n"
          "Cache-Control: no-store\r\n\r\n" + GetHTML();
  }

  static std::string GetImageResponse() {
      return "HTTP/1.1 200 OK\r\n"
          "Content-Type: image/png\r\n"
          "Cache-Control: no-store\r\n\r\n" + GetImage();
  }

  static std::string GetImage() {
    return ResourceBundle::GetSharedInstance().
        GetDataResource(IDR_BLACKLIST_IMAGE);
  }

  std::string GetHTML() const {
    DictionaryValue strings;
    strings.SetString(L"title", l10n_util::GetString(IDS_BLACKLIST_TITLE));
    strings.SetString(L"message", l10n_util::GetString(IDS_BLACKLIST_MESSAGE));

    const Blacklist::Provider* provider = GetBestMatchingEntryProvider();
    strings.SetString(L"name", provider->name());
    strings.SetString(L"url", provider->url());

    const base::StringPiece html =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLACKLIST_HTML);
    return jstemplate_builder::GetI18nTemplateHtml(html, &strings);
  }

  const Blacklist::Provider* GetBestMatchingEntryProvider() const {
    // If kBlockAll is specified, assign blame to such an entry.
    // Otherwise pick the first one.
    const BlacklistManager* blacklist_manager =
        request_info_.GetBlacklistManager();
    const Blacklist* blacklist = blacklist_manager->GetCompiledBlacklist();
    scoped_ptr<Blacklist::Match> match(blacklist->FindMatch(request_->url()));
    const Blacklist::Entry* entry = NULL;
    if (match->attributes() & Blacklist::kBlockAll) {
      for (std::vector<const Blacklist::Entry*>::const_iterator i =
           match->entries().begin(); i != match->entries().end(); ++i) {
        if ((*i)->attributes() == Blacklist::kBlockAll) {
          entry = *i;
          break;
        }
      }
    } else {
      entry = match->entries().front();
    }
    return entry->provider();
  }

  const BlacklistRequestInfo& request_info_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestBlacklistJob);
};

}  // namespace

BlacklistInterceptor::BlacklistInterceptor() {
  URLRequest::RegisterRequestInterceptor(this);
}

BlacklistInterceptor::~BlacklistInterceptor() {
  URLRequest::UnregisterRequestInterceptor(this);
}

URLRequestJob* BlacklistInterceptor::MaybeIntercept(URLRequest* request) {
  BlacklistRequestInfo* request_info =
      BlacklistRequestInfo::FromURLRequest(request);
  if (!request_info) {
    // Not all requests have privacy blacklist data, for example downloads.
    return NULL;
  }

  const BlacklistManager* blacklist_manager =
      request_info->GetBlacklistManager();
  const Blacklist* blacklist = blacklist_manager->GetCompiledBlacklist();
  scoped_ptr<Blacklist::Match> match(blacklist->FindMatch(request->url()));

  if (!match.get()) {
    // Nothing is blacklisted for this request. Do not intercept.
    return NULL;
  }

  // TODO(phajdan.jr): Should we have some UI to notify about blocked referrer?
  if (match->attributes() & Blacklist::kDontSendReferrer)
    request->set_referrer(std::string());

  if (match->IsBlocked(request->url()))
    return new URLRequestBlacklistJob(request, *request_info);

  return NULL;
}
