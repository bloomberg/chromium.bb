// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/web_apps.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/web_application_info.h"
#include "third_party/WebKit/public/platform/WebIconSizesParser.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebNode;
using blink::WebString;

namespace web_apps {
namespace {

// Sizes a single size (the width or height) from a 'sizes' attribute. A size
// matches must match the following regex: [1-9][0-9]*.
int ParseSingleIconSize(const base::StringPiece16& text) {
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
  std::vector<base::StringPiece16> sizes = base::SplitStringPiece(
      text, base::string16(1, 'x'),
      base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
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
  if (link.hasAttribute("sizes")) {
    blink::WebVector<blink::WebSize> icon_sizes =
        blink::WebIconSizesParser::parseIconSizes(link.getAttribute("sizes"));
    if (icon_sizes.size() == 1 &&
        icon_sizes[0].width != 0 &&
        icon_sizes[0].height != 0) {
      icon_info.width = icon_sizes[0].width;
      icon_info.height = icon_sizes[0].height;
    }
  }
  icon_info.url = url;
  icons->push_back(icon_info);
}

}  // namespace

bool ParseIconSizes(const base::string16& text,
                    std::vector<gfx::Size>* sizes,
                    bool* is_any) {
  *is_any = false;
  std::vector<base::string16> size_strings = base::SplitString(
      text, base::kWhitespaceASCIIAs16,
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (size_t i = 0; i < size_strings.size(); ++i) {
    if (base::EqualsASCII(size_strings[i], "any")) {
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

void ParseWebAppFromWebDocument(WebFrame* frame,
                                WebApplicationInfo* app_info) {
  WebDocument document = frame->document();
  if (document.isNull())
    return;

  WebElement head = document.head();
  if (head.isNull())
    return;

  GURL document_url = document.url();
  for (WebNode child = head.firstChild(); !child.isNull();
      child = child.nextSibling()) {
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
      // Bookmark apps also support "apple-touch-icon" and
      // "apple-touch-icon-precomposed".
#if defined(OS_MACOSX)
      bool bookmark_apps_enabled = base::CommandLine::ForCurrentProcess()->
          HasSwitch(switches::kEnableNewBookmarkApps);
#else
      bool bookmark_apps_enabled = !base::CommandLine::ForCurrentProcess()->
          HasSwitch(switches::kDisableNewBookmarkApps);
#endif
      if (base::LowerCaseEqualsASCII(rel, "icon") ||
          base::LowerCaseEqualsASCII(rel, "shortcut icon") ||
          (bookmark_apps_enabled &&
           (base::LowerCaseEqualsASCII(rel, "apple-touch-icon") ||
            base::LowerCaseEqualsASCII(rel, "apple-touch-icon-precomposed")))) {
        AddInstallIcon(elem, &app_info->icons);
      }
    } else if (elem.hasHTMLTagName("meta") && elem.hasAttribute("name")) {
      std::string name = elem.getAttribute("name").utf8();
      WebString content = elem.getAttribute("content");
      if (name == "application-name") {
        app_info->title = content.utf16();
      } else if (name == "description") {
        app_info->description = content.utf16();
      } else if (name == "application-url") {
        std::string url = content.utf8();
        app_info->app_url = document_url.is_valid() ?
            document_url.Resolve(url) : GURL(url);
        if (!app_info->app_url.is_valid())
          app_info->app_url = GURL();
      } else if (name == "mobile-web-app-capable" &&
                 base::LowerCaseEqualsASCII(content.utf16(), "yes")) {
        app_info->mobile_capable = WebApplicationInfo::MOBILE_CAPABLE;
      } else if (name == "apple-mobile-web-app-capable" &&
                 base::LowerCaseEqualsASCII(content.utf16(), "yes") &&
                 app_info->mobile_capable ==
                     WebApplicationInfo::MOBILE_CAPABLE_UNSPECIFIED) {
        app_info->mobile_capable = WebApplicationInfo::MOBILE_CAPABLE_APPLE;
      }
    }
  }
}

}  // namespace web_apps
