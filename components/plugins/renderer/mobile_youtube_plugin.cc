// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/mobile_youtube_plugin.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "ui/base/webui/jstemplate_builder.h"

using blink::WebFrame;
using blink::WebPlugin;
using blink::WebURLRequest;

const char* const kSlashVSlash = "/v/";
const char* const kSlashESlash = "/e/";

namespace {

std::string GetYoutubeVideoId(const blink::WebPluginParams& params) {
  GURL url(params.url);
  std::string video_id = url.path().substr(strlen(kSlashVSlash));

  // Extract just the video id
  size_t video_id_end = video_id.find('&');
  if (video_id_end != std::string::npos)
    video_id = video_id.substr(0, video_id_end);
  return video_id;
}

std::string HtmlData(const blink::WebPluginParams& params,
                     base::StringPiece template_html) {
  base::DictionaryValue values;
  values.SetString("video_id", GetYoutubeVideoId(params));
  return webui::GetI18nTemplateHtml(template_html, &values);
}

bool IsValidYouTubeVideo(const std::string& path) {
  unsigned len = strlen(kSlashVSlash);

  // check for more than just /v/ or /e/.
  if (path.length() <= len)
    return false;

  std::string str = base::StringToLowerASCII(path);
  // Youtube flash url can start with /v/ or /e/.
  if (strncmp(str.data(), kSlashVSlash, len) != 0 &&
      strncmp(str.data(), kSlashESlash, len) != 0)
    return false;

  // Start after /v/
  for (unsigned i = len; i < path.length(); i++) {
    char c = str[i];
    if (isalpha(c) || isdigit(c) || c == '_' || c == '-')
      continue;
    // The url can have more parameters such as &hl=en after the video id.
    // Once we start seeing extra parameters we can return true.
    return c == '&' && i > len;
  }
  return true;
}

}  // namespace

namespace plugins {

MobileYouTubePlugin::MobileYouTubePlugin(content::RenderFrame* render_frame,
                                         blink::WebLocalFrame* frame,
                                         const blink::WebPluginParams& params,
                                         base::StringPiece& template_html,
                                         GURL placeholderDataUrl)
    : PluginPlaceholder(render_frame,
                        frame,
                        params,
                        HtmlData(params, template_html),
                        placeholderDataUrl) {}

MobileYouTubePlugin::~MobileYouTubePlugin() {}

// static
bool MobileYouTubePlugin::IsYouTubeURL(const GURL& url,
                                       const std::string& mime_type) {
  std::string host = url.host();
  bool is_youtube = EndsWith(host, "youtube.com", true) ||
                    EndsWith(host, "youtube-nocookie.com", true);

  return is_youtube && IsValidYouTubeVideo(url.path()) &&
         base::LowerCaseEqualsASCII(mime_type,
                                    content::kFlashPluginSwfMimeType);
}

void MobileYouTubePlugin::OpenYoutubeUrlCallback() {
  std::string youtube("vnd.youtube:");
  GURL url(youtube.append(GetYoutubeVideoId(GetPluginParams())));
  WebURLRequest request;
  request.initialize();
  request.setURL(url);
  render_frame()->LoadURLExternally(
      GetFrame(), request, blink::WebNavigationPolicyNewForegroundTab);
}

void MobileYouTubePlugin::BindWebFrame(WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  DCHECK(!context.IsEmpty());

  v8::Context::Scope context_scope(context);
  v8::Handle<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "plugin"),
              gin::CreateHandle(isolate, this).ToV8());
}

gin::ObjectTemplateBuilder MobileYouTubePlugin::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return PluginPlaceholder::GetObjectTemplateBuilder(isolate)
    .SetMethod("openYoutubeURL", &MobileYouTubePlugin::OpenYoutubeUrlCallback);
}

}  // namespace plugins
