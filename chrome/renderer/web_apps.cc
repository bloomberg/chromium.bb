// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/web_apps.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/web_application_info.h"
#include "grit/common_resources.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebNode;
using blink::WebNodeList;
using blink::WebString;

namespace web_apps {
namespace {

// Sizes a single size (the width or height) from a 'sizes' attribute. A size
// matches must match the following regex: [1-9][0-9]*.
int ParseSingleIconSize(const base::string16& text) {
  // Size must not start with 0, and be between 0 and 9.
  if (text.empty() || !(text[0] >= L'1' && text[0] <= L'9'))
    return 0;

  // Make sure all chars are from 0-9.
  for (size_t i = 1; i < text.length(); ++i) {
    if (!(text[i] >= L'0' && text[i] <= L'9'))
      return 0;
  }
  int output;
  if (!base::StringToInt(text, &output))
    return 0;
  return output;
}

// Parses an icon size. An icon size must match the following regex:
// [1-9][0-9]*x[1-9][0-9]*.
// If the input couldn't be parsed, a size with a width/height == 0 is returned.
gfx::Size ParseIconSize(const base::string16& text) {
  std::vector<base::string16> sizes;
  base::SplitStringDontTrim(text, L'x', &sizes);
  if (sizes.size() != 2)
    return gfx::Size();

  return gfx::Size(ParseSingleIconSize(sizes[0]),
                   ParseSingleIconSize(sizes[1]));
}

void AddInstallIcon(const WebElement& link,
                    std::vector<WebApplicationInfo::IconInfo>* icons) {
  WebString href = link.getAttribute("href");
  if (href.isNull() || href.isEmpty())
    return;

  // Get complete url.
  GURL url = link.document().completeURL(href);
  if (!url.is_valid())
    return;

  WebApplicationInfo::IconInfo icon_info;
  bool is_any = false;
  std::vector<gfx::Size> icon_sizes;
  if (link.hasAttribute("sizes") &&
      ParseIconSizes(link.getAttribute("sizes"), &icon_sizes, &is_any) &&
      !is_any &&
      icon_sizes.size() == 1) {
    icon_info.width = icon_sizes[0].width();
    icon_info.height = icon_sizes[0].height();
  }
  icon_info.url = url;
  icons->push_back(icon_info);
}

}  // namespace

bool ParseIconSizes(const base::string16& text,
                    std::vector<gfx::Size>* sizes,
                    bool* is_any) {
  *is_any = false;
  std::vector<base::string16> size_strings;
  base::SplitStringAlongWhitespace(text, &size_strings);
  for (size_t i = 0; i < size_strings.size(); ++i) {
    if (EqualsASCII(size_strings[i], "any")) {
      *is_any = true;
    } else {
      gfx::Size size = ParseIconSize(size_strings[i]);
      if (size.width() <= 0 || size.height() <= 0)
        return false;  // Bogus size.
      sizes->push_back(size);
    }
  }
  if (*is_any && !sizes->empty()) {
    // If is_any is true, it must occur by itself.
    return false;
  }
  return (*is_any || !sizes->empty());
}

bool ParseWebAppFromWebDocument(WebFrame* frame,
                                WebApplicationInfo* app_info,
                                base::string16* error) {
  WebDocument document = frame->document();
  if (document.isNull())
    return true;

  WebElement head = document.head();
  if (head.isNull())
    return true;

  GURL document_url = document.url();
  WebNodeList children = head.childNodes();
  for (unsigned i = 0; i < children.length(); ++i) {
    WebNode child = children.item(i);
    if (!child.isElementNode())
      continue;
    WebElement elem = child.to<WebElement>();

    if (elem.hasHTMLTagName("link")) {
      std::string rel = elem.getAttribute("rel").utf8();
      // "rel" attribute may use either "icon" or "shortcut icon".
      // see also
      //   <http://en.wikipedia.org/wiki/Favicon>
      //   <http://dev.w3.org/html5/spec/Overview.html#rel-icon>
      //
      // Streamlined Hosted Apps also support "apple-touch-icon" and
      // "apple-touch-icon-precomposed".
      if (LowerCaseEqualsASCII(rel, "icon") ||
          LowerCaseEqualsASCII(rel, "shortcut icon") ||
          (CommandLine::ForCurrentProcess()->
              HasSwitch(switches::kEnableStreamlinedHostedApps) &&
            (LowerCaseEqualsASCII(rel, "apple-touch-icon") ||
             LowerCaseEqualsASCII(rel, "apple-touch-icon-precomposed")))) {
        AddInstallIcon(elem, &app_info->icons);
      }
    } else if (elem.hasHTMLTagName("meta") && elem.hasAttribute("name")) {
      std::string name = elem.getAttribute("name").utf8();
      WebString content = elem.getAttribute("content");
      if (name == "application-name") {
        app_info->title = content;
      } else if (name == "description") {
        app_info->description = content;
      } else if (name == "application-url") {
        std::string url = content.utf8();
        app_info->app_url = document_url.is_valid() ?
            document_url.Resolve(url) : GURL(url);
        if (!app_info->app_url.is_valid())
          app_info->app_url = GURL();
      }
    }
  }

  return true;
}

}  // namespace web_apps
