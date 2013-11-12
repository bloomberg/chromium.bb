// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/thumbnail_list_source.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"

namespace {

const char* html_header =
    "<!DOCTYPE HTML>\n<html>\n<head>\n<title>TopSites Thumbnails</title>\n"
    "<meta charset=\"utf-8\">\n"
    "<style type=\"text/css\">\nimg.thumb {border: 1px solid black;}\n"
    "li {white-space: nowrap;}\n</style>\n";
const char* html_body = "</head>\n<body>\n";
const char* html_footer = "</body>\n</html>\n";

// If |want_thumbnails| == true, then renders elements in |mvurl_list| that have
// thumbnails, with their thumbnails. Otherwise renders elements in |mvurl_list|
// that have no thumbnails.
void RenderMostVisitedURLList(
    const history::MostVisitedURLList& mvurl_list,
    const std::vector<std::string>& base64_encoded_pngs,
    bool want_thumbnails,
    std::vector<std::string>* out) {
  DCHECK_EQ(mvurl_list.size(), base64_encoded_pngs.size());
  out->push_back("<div><ul>\n");
  for (size_t i = 0; i < mvurl_list.size(); ++i) {
    const history::MostVisitedURL& mvurl = mvurl_list[i];
    bool has_thumbnail = !base64_encoded_pngs[i].empty();
    if (has_thumbnail == want_thumbnails) {
      out->push_back("<li>\n");
      out->push_back(net::EscapeForHTML(mvurl.url.spec()) + "\n");
      if (want_thumbnails) {
        out->push_back("<div><img class=\"thumb\" "
                       "src=\"data:image/png;base64," +
                       base64_encoded_pngs[i] + "\"/></div>\n");
      }
      if (!mvurl.redirects.empty()) {
        out->push_back("<ul>\n");
        history::RedirectList::const_iterator jt;
        for (jt = mvurl.redirects.begin();
             jt != mvurl.redirects.end(); ++jt) {
          out->push_back("<li>" + net::EscapeForHTML(jt->spec()) + "</li>\n");
        }
        out->push_back("</ul>\n");
      }
      out->push_back("</li>\n");
    }
  }
  out->push_back("</ul></div>\n");
}

}  // namespace

ThumbnailListSource::ThumbnailListSource(Profile* profile)
    : thumbnail_service_(ThumbnailServiceFactory::GetForProfile(profile)),
      profile_(profile),
      weak_ptr_factory_(this) {
}

ThumbnailListSource::~ThumbnailListSource() {
}

std::string ThumbnailListSource::GetSource() const {
  return chrome::kChromeUIThumbnailListHost;
}

void ThumbnailListSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_view_id,
    const content::URLDataSource::GotDataCallback& callback) {
  profile_->GetTopSites()->GetMostVisitedURLs(
      base::Bind(&ThumbnailListSource::OnMostVisitedURLsAvailable,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback), false);
}

std::string ThumbnailListSource::GetMimeType(const std::string& path) const {
  return "text/html";
}

base::MessageLoop* ThumbnailListSource::MessageLoopForRequestPath(
    const std::string& path) const {
  // TopSites can be accessed from the IO thread.
  return thumbnail_service_.get() ?
      NULL : content::URLDataSource::MessageLoopForRequestPath(path);
}

bool ThumbnailListSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  if (request->url().SchemeIs(chrome::kChromeSearchScheme))
    return InstantIOContext::ShouldServiceRequest(request);
  return URLDataSource::ShouldServiceRequest(request);
}

void ThumbnailListSource::OnMostVisitedURLsAvailable(
    const content::URLDataSource::GotDataCallback& callback,
    const history::MostVisitedURLList& mvurl_list) {
  const size_t num_mv = mvurl_list.size();
  size_t num_mv_with_thumb = 0;

  // Encode all available thumbnails and store into |base64_encoded_pngs|.
  std::vector<std::string> base64_encoded_pngs(num_mv);
  for (size_t i = 0; i < num_mv; ++i) {
    scoped_refptr<base::RefCountedMemory> data;
    if (thumbnail_service_->GetPageThumbnail(mvurl_list[i].url, false, &data)) {
      std::string data_str;
      data_str.assign(reinterpret_cast<const char*>(data->front()),
                      data->size());
      base::Base64Encode(data_str, &base64_encoded_pngs[i]);
      ++num_mv_with_thumb;
    }
  }

  // Render HTML to embed URLs and thumbnails.
  std::vector<std::string> out;
  out.push_back(html_header);
  out.push_back(html_body);
  if (num_mv_with_thumb > 0) {
    out.push_back("<h2>TopSites URLs with Thumbnails</h2>\n");
    RenderMostVisitedURLList(mvurl_list, base64_encoded_pngs, true, &out);
  }
  if (num_mv_with_thumb < num_mv) {
    out.push_back("<h2>TopSites URLs without Thumbnails</h2>\n");
    RenderMostVisitedURLList(mvurl_list, base64_encoded_pngs, false, &out);
  }
  out.push_back(html_footer);

  std::string out_html = JoinString(out, "");
  callback.Run(base::RefCountedString::TakeString(&out_html));
}
