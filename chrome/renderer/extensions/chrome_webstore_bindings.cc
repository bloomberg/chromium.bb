// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_webstore_bindings.h"

#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "content/renderer/render_view.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeList.h"
#include "v8/include/v8.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebNodeList;

const char kWebstoreLinkRelation[] = "chrome-webstore-item";

const char kNotInTopFrameError[] =
    "Chrome Web Store installations can only be started by the top frame.";
const char kNotUserGestureError[] =
    "Chrome Web Store installations can only be initated by a user gesture.";
const char kNoWebstoreItemLinkFoundError[] =
    "No Chrome Web Store item link found.";
const char kInvalidWebstoreItemUrlError[] =
    "Invalid Chrome Web Store item URL.";

namespace extensions_v8 {

static const char* const kWebstoreExtensionName = "v8/ChromeWebstore";

class ChromeWebstoreExtensionWrapper : public v8::Extension {
 public:
  ChromeWebstoreExtensionWrapper() :
    v8::Extension(
        kWebstoreExtensionName,
        "var chrome;"
        "if (!chrome)"
        "  chrome = {};"
        "if (!chrome.webstore) {"
        "  chrome.webstore = new function() {"
        "    native function Install();"
        "    this.install = Install;"
        "  };"
        "}") {
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("Install"))) {
      return v8::FunctionTemplate::New(Install);
    } else {
      return v8::Handle<v8::FunctionTemplate>();
    }
  }

  static v8::Handle<v8::Value> Install(const v8::Arguments& args) {
    WebFrame* frame = WebFrame::frameForCurrentContext();
    RenderView* render_view = bindings_utils::GetRenderViewForCurrentContext();
    if (frame && render_view) {
      std::string webstore_item_id;
      std::string error;
      if (GetWebstoreItemIdFromFrame(frame, &webstore_item_id, &error)) {
        ExtensionHelper* helper = ExtensionHelper::Get(render_view);
        helper->InlineWebstoreInstall(webstore_item_id);
      } else {
        v8::ThrowException(v8::String::New(error.c_str()));
      }
    }

    return v8::Undefined();
  }

 private:
  // Extracts a Web Store item ID from a <link rel="chrome-webstore-item"
  // href="https://chrome.google.com/webstore/detail/id"> node found in the
  // frame. On success, true will be returned and the |webstore_item_id|
  // parameter will be populated with the ID. On failure, false will be returned
  // and |error| will be populated with the error.
  static bool GetWebstoreItemIdFromFrame(
      WebFrame* frame, std::string* webstore_item_id, std::string* error) {
    if (frame != frame->top()) {
      *error = kNotInTopFrameError;
      return false;
    }

    if (!frame->isProcessingUserGesture()) {
      *error = kNotUserGestureError;
      return false;
    }

    WebDocument document = frame->document();
    if (document.isNull()) {
      *error = kNoWebstoreItemLinkFoundError;
      return false;
    }

    WebElement head = document.head();
    if (head.isNull()) {
      *error = kNoWebstoreItemLinkFoundError;
      return false;
    }

    GURL webstore_base_url =
        GURL(extension_misc::GetWebstoreItemDetailURLPrefix());
    WebNodeList children = head.childNodes();
    for (unsigned i = 0; i < children.length(); ++i) {
      WebNode child = children.item(i);
      if (!child.isElementNode())
        continue;
      WebElement elem = child.to<WebElement>();

      if (!elem.hasTagName("link") || !elem.hasAttribute("rel") ||
          !elem.hasAttribute("href"))
        continue;

      std::string rel = elem.getAttribute("rel").utf8();
      if (!LowerCaseEqualsASCII(rel, kWebstoreLinkRelation))
        continue;

      std::string webstore_url_string(elem.getAttribute("href").utf8());
      GURL webstore_url = GURL(webstore_url_string);
      if (!webstore_url.is_valid()) {
        *error = kInvalidWebstoreItemUrlError;
        return false;
      }

      if (webstore_url.scheme() != webstore_base_url.scheme() ||
          webstore_url.host() != webstore_base_url.host() ||
          !StartsWithASCII(
              webstore_url.path(), webstore_base_url.path(), true)) {
        *error = kInvalidWebstoreItemUrlError;
        return false;
      }

      std::string candidate_webstore_item_id = webstore_url.path().substr(
          webstore_base_url.path().length());
      if (!::Extension::IdIsValid(candidate_webstore_item_id)) {
        *error = kInvalidWebstoreItemUrlError;
        return false;
      }

      std::string reconstructed_webstore_item_url_string =
          extension_misc::GetWebstoreItemDetailURLPrefix() +
              candidate_webstore_item_id;
      if (reconstructed_webstore_item_url_string != webstore_url_string) {
        *error = kInvalidWebstoreItemUrlError;
        return false;
      }

      *webstore_item_id = candidate_webstore_item_id;
      return true;
    }

    *error = kNoWebstoreItemLinkFoundError;
    return false;
  }
};

v8::Extension* ChromeWebstoreExtension::Get() {
  return new ChromeWebstoreExtensionWrapper();
}

}  // namespace extensions_v8
