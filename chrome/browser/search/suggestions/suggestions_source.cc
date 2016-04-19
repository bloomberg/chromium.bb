// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_source.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/suggestions/suggestions_service.h"
#include "net/base/escape.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace suggestions {

namespace {

const char kHtmlHeader[] =
    "<!DOCTYPE html>\n<html>\n<head>\n<title>Suggestions</title>\n"
    "<meta charset=\"utf-8\">\n"
    "<style type=\"text/css\">\nli {white-space: nowrap;}\n</style>\n";
const char kHtmlBody[] = "</head>\n<body>\n";
const char kHtmlFooter[] = "</body>\n</html>\n";

const char kRefreshPath[] = "refresh";

std::string GetRefreshHtml(bool is_refresh) {
  if (is_refresh)
    return "<p>Refreshing in the background, reload to see new data.</p>\n";
  return std::string("<p><a href=\"") + chrome::kChromeUISuggestionsURL +
         kRefreshPath + "\">Refresh</a></p>\n";
}
// Returns the HTML needed to display the suggestions.
std::string RenderOutputHtml(
    bool is_refresh,
    const SuggestionsProfile& profile,
    const std::map<GURL, std::string>& base64_encoded_pngs) {
  std::vector<std::string> out;
  out.push_back(kHtmlHeader);
  out.push_back(kHtmlBody);
  out.push_back("<h1>Suggestions</h1>\n");
  out.push_back(GetRefreshHtml(is_refresh));
  out.push_back("<ul>");
  int64_t now = (base::Time::NowFromSystemTime() - base::Time::UnixEpoch())
                    .ToInternalValue();
  size_t size = profile.suggestions_size();
  for (size_t i = 0; i < size; ++i) {
    const ChromeSuggestion& suggestion = profile.suggestions(i);
    base::TimeDelta remaining_time = base::TimeDelta::FromMicroseconds(
        suggestion.expiry_ts() - now);
    base::string16 remaining_time_formatted = ui::TimeFormat::Detailed(
        ui::TimeFormat::Format::FORMAT_DURATION,
        ui::TimeFormat::Length::LENGTH_LONG,
        -1, remaining_time);
    std::string line;
    line += "<li><a href=\"";
    line += net::EscapeForHTML(suggestion.url());
    line += "\" target=\"_blank\">";
    line += net::EscapeForHTML(suggestion.title());
    std::map<GURL, std::string>::const_iterator it =
        base64_encoded_pngs.find(GURL(suggestion.url()));
    if (it != base64_encoded_pngs.end()) {
      line += "<br><img src='";
      line += it->second;
      line += "'>";
    }
    line += "</a> Expires in ";
    line += base::UTF16ToUTF8(remaining_time_formatted);
    std::vector<std::string> providers;
    for (int p = 0; p < suggestion.providers_size(); ++p)
      providers.push_back(base::IntToString(suggestion.providers(p)));
    line += ". Provider IDs: " + base::JoinString(providers, ", ");
    line += "</li>\n";
    out.push_back(line);
  }
  out.push_back("</ul>");
  out.push_back(kHtmlFooter);
  return base::JoinString(out, base::StringPiece());
}

// Returns the HTML needed to display that no suggestions are available.
std::string RenderOutputHtmlNoSuggestions(bool is_refresh) {
  std::vector<std::string> out;
  out.push_back(kHtmlHeader);
  out.push_back(kHtmlBody);
  out.push_back("<h1>Suggestions</h1>\n");
  out.push_back("<p>You have no suggestions.</p>\n");
  out.push_back(GetRefreshHtml(is_refresh));
  out.push_back(kHtmlFooter);
  return base::JoinString(out, base::StringPiece());
}

}  // namespace

SuggestionsSource::SuggestionsSource(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

SuggestionsSource::~SuggestionsSource() {}

SuggestionsSource::RequestContext::RequestContext(
    bool is_refresh_in,
    const SuggestionsProfile& suggestions_profile_in,
    const content::URLDataSource::GotDataCallback& callback_in)
    : is_refresh(is_refresh_in),
      suggestions_profile(suggestions_profile_in),  // Copy.
      callback(callback_in)                         // Copy.
{}

SuggestionsSource::RequestContext::~RequestContext() {}

std::string SuggestionsSource::GetSource() const {
  return chrome::kChromeUISuggestionsHost;
}

void SuggestionsSource::StartDataRequest(
    const std::string& path, int render_process_id, int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  // If this was called as "chrome://suggestions/refresh", we also trigger an
  // async update of the suggestions.
  bool is_refresh = (path == kRefreshPath);

  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForProfile(profile_);

  // |suggestions_service| is null for guest profiles.
  if (!suggestions_service) {
    std::string output = RenderOutputHtmlNoSuggestions(is_refresh);
    callback.Run(base::RefCountedString::TakeString(&output));
    return;
  }

  if (is_refresh)
    suggestions_service->FetchSuggestionsData();

  SuggestionsProfile suggestions_profile =
      suggestions_service->GetSuggestionsDataFromCache();
  size_t size = suggestions_profile.suggestions_size();
  if (!size) {
    std::string output = RenderOutputHtmlNoSuggestions(is_refresh);
    callback.Run(base::RefCountedString::TakeString(&output));
  } else {
    RequestContext* context =
        new RequestContext(is_refresh, suggestions_profile, callback);
    base::Closure barrier = BarrierClosure(
        size, base::Bind(&SuggestionsSource::OnThumbnailsFetched,
                         weak_ptr_factory_.GetWeakPtr(), context));
    for (size_t i = 0; i < size; ++i) {
      const ChromeSuggestion& suggestion = suggestions_profile.suggestions(i);
      // Fetch the thumbnail for this URL (exercising the fetcher). After all
      // fetches are done, including NULL callbacks for unavailable thumbnails,
      // SuggestionsSource::OnThumbnailsFetched will be called.
      SuggestionsService* suggestions_service(
          SuggestionsServiceFactory::GetForProfile(profile_));
      suggestions_service->GetPageThumbnail(
          GURL(suggestion.url()),
          base::Bind(&SuggestionsSource::OnThumbnailAvailable,
                     weak_ptr_factory_.GetWeakPtr(), context, barrier));
    }
  }
}

std::string SuggestionsSource::GetMimeType(const std::string& path) const {
  return "text/html";
}

base::MessageLoop* SuggestionsSource::MessageLoopForRequestPath(
    const std::string& path) const {
  // This can be accessed from the IO thread.
  return content::URLDataSource::MessageLoopForRequestPath(path);
}

void SuggestionsSource::OnThumbnailsFetched(RequestContext* context) {
  std::unique_ptr<RequestContext> context_deleter(context);

  std::string output =
      RenderOutputHtml(context->is_refresh, context->suggestions_profile,
                       context->base64_encoded_pngs);
  context->callback.Run(base::RefCountedString::TakeString(&output));
}

void SuggestionsSource::OnThumbnailAvailable(RequestContext* context,
                                             base::Closure barrier,
                                             const GURL& url,
                                             const SkBitmap* bitmap) {
  if (bitmap) {
    std::vector<unsigned char> output;
    gfx::PNGCodec::EncodeBGRASkBitmap(*bitmap, false, &output);

    std::string encoded_output;
    base::Base64Encode(
        base::StringPiece(reinterpret_cast<const char*>(output.data()),
                          output.size()),
        &encoded_output);
    context->base64_encoded_pngs[url] = "data:image/png;base64,";
    context->base64_encoded_pngs[url] += encoded_output;
  }
  barrier.Run();
}

}  // namespace suggestions
