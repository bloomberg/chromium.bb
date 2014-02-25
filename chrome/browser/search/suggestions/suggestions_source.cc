// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_source.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "chrome/browser/search/suggestions/suggestions_service.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace suggestions {

namespace {

const char kHtmlHeader[] =
    "<!DOCTYPE html>\n<html>\n<head>\n<title>Suggestions</title>\n"
    "<meta charset=\"utf-8\">\n"
    "<style type=\"text/css\">\nli {white-space: nowrap;}\n</style>\n";
const char kHtmlBody[] = "</head>\n<body>\n";
const char kHtmlFooter[] = "</body>\n</html>\n";

}  // namespace

SuggestionsSource::SuggestionsSource(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
}

SuggestionsSource::~SuggestionsSource() {
}

std::string SuggestionsSource::GetSource() const {
  return chrome::kChromeUISuggestionsHost;
}

void SuggestionsSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  SuggestionsServiceFactory* suggestions_service_factory =
      SuggestionsServiceFactory::GetInstance();

  SuggestionsService* suggestions_service(
      suggestions_service_factory->GetForProfile(profile_));

  // If SuggestionsService is unavailable, then SuggestionsSource should not
  // have been instantiated in the first place.
  DCHECK(suggestions_service != NULL);

  suggestions_service->FetchSuggestionsData(
      base::Bind(&SuggestionsSource::OnSuggestionsAvailable,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

std::string SuggestionsSource::GetMimeType(const std::string& path) const {
  return "text/html";
}

base::MessageLoop* SuggestionsSource::MessageLoopForRequestPath(
    const std::string& path) const {
  // This can be accessed from the IO thread.
  return content::URLDataSource::MessageLoopForRequestPath(path);
}

bool SuggestionsSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  if (request->url().SchemeIs(chrome::kChromeSearchScheme))
    return InstantIOContext::ShouldServiceRequest(request);
  return URLDataSource::ShouldServiceRequest(request);
}

void SuggestionsSource::OnSuggestionsAvailable(
    const content::URLDataSource::GotDataCallback& callback,
    const SuggestionsProfile& suggestions_profile) {
  // Render HTML.
  std::vector<std::string> out;
  out.push_back(kHtmlHeader);
  out.push_back(kHtmlBody);
  out.push_back("<h1>Suggestions</h1>\n");

  size_t size = suggestions_profile.suggestions_size();
  if (size == 0) {
    out.push_back("<p>You have no suggestions.</p>\n");
  } else {
    out.push_back("<ol>\n");
    for (size_t i = 0; i < size; ++i) {
      const ChromeSuggestion& suggestion = suggestions_profile.suggestions(i);
      std::string line;
      line.append("<li><a href=\"");
      line.append(net::EscapeForHTML(suggestion.url()));
      line.append("\" target=\"_blank\">");
      line.append(net::EscapeForHTML(suggestion.title()));
      line.append("</a></li>\n");
      out.push_back(line);
    }
    out.push_back("</ol>\n");
  }
  out.push_back(kHtmlFooter);

  std::string out_html = JoinString(out, "");
  callback.Run(base::RefCountedString::TakeString(&out_html));
}

}  // namespace suggestions
