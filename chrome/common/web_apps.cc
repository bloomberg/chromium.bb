// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/web_apps.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/json_schema_validator.h"
#include "googleurl/src/gurl.h"
#include "grit/common_resources.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeList.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"
#include "webkit/glue/dom_operations.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebNodeList;
using WebKit::WebString;

namespace {

// Sizes a single size (the width or height) from a 'sizes' attribute. A size
// matches must match the following regex: [1-9][0-9]*.
static int ParseSingleIconSize(const string16& text) {
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

void AddInstallIcon(const WebElement& link,
                    std::vector<WebApplicationInfo::IconInfo>* icons) {
  WebString href = link.getAttribute("href");
  if (href.isNull() || href.isEmpty())
    return;

  // Get complete url.
  GURL url = link.document().completeURL(href);
  if (!url.is_valid())
    return;

  if (!link.hasAttribute("sizes"))
    return;

  bool is_any = false;
  std::vector<gfx::Size> icon_sizes;
  if (!web_apps::ParseIconSizes(link.getAttribute("sizes"), &icon_sizes,
                                &is_any) ||
      is_any ||
      icon_sizes.size() != 1) {
    return;
  }
  WebApplicationInfo::IconInfo icon_info;
  icon_info.width = icon_sizes[0].width();
  icon_info.height = icon_sizes[0].height();
  icon_info.url = url;
  icons->push_back(icon_info);
}

}  // namespace

const char WebApplicationInfo::kInvalidDefinitionURL[] =
    "Invalid application definition URL. Must be a valid relative URL or "
    "an absolute URL with the same origin as the document.";
const char WebApplicationInfo::kInvalidLaunchURL[] =
    "Invalid value for property 'launch_url'. Must be a valid relative URL or "
    "an absolute URL with the same origin as the application definition.";
const char WebApplicationInfo::kInvalidURL[] =
    "Invalid value for property 'urls[*]'. Must be a valid relative URL or "
    "an absolute URL with the same origin as the application definition.";
const char WebApplicationInfo::kInvalidIconURL[] =
    "Invalid value for property 'icons.*'. Must be a valid relative URL or "
    "an absolute URL with the same origin as the application definition.";

WebApplicationInfo::WebApplicationInfo() {
  is_bookmark_app = false;
  is_offline_enabled = false;
}

WebApplicationInfo::~WebApplicationInfo() {
}


namespace web_apps {

gfx::Size ParseIconSize(const string16& text) {
  std::vector<string16> sizes;
  base::SplitStringDontTrim(text, L'x', &sizes);
  if (sizes.size() != 2)
    return gfx::Size();

  return gfx::Size(ParseSingleIconSize(sizes[0]),
                   ParseSingleIconSize(sizes[1]));
}

bool ParseIconSizes(const string16& text,
                    std::vector<gfx::Size>* sizes,
                    bool* is_any) {
  *is_any = false;
  std::vector<string16> size_strings;
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
                                string16* error) {
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

    if (elem.hasTagName("link")) {
      std::string rel = elem.getAttribute("rel").utf8();
      // "rel" attribute may use either "icon" or "shortcut icon".
      // see also
      //   <http://en.wikipedia.org/wiki/Favicon>
      //   <http://dev.w3.org/html5/spec/Overview.html#rel-icon>
      if (LowerCaseEqualsASCII(rel, "icon") ||
          LowerCaseEqualsASCII(rel, "shortcut icon")) {
        AddInstallIcon(elem, &app_info->icons);
      } else if (LowerCaseEqualsASCII(rel, "chrome-application-definition")) {
        std::string definition_url_string(elem.getAttribute("href").utf8());
        GURL definition_url;
        if (!(definition_url =
              document_url.Resolve(definition_url_string)).is_valid() ||
            definition_url.GetOrigin() != document_url.GetOrigin()) {
          *error = UTF8ToUTF16(WebApplicationInfo::kInvalidDefinitionURL);
          return false;
        }

        // If there is a definition file, all attributes come from it.
        *app_info = WebApplicationInfo();
        app_info->manifest_url = definition_url;
        return true;
      }
    } else if (elem.hasTagName("meta") && elem.hasAttribute("name")) {
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

bool ParseWebAppFromDefinitionFile(Value* definition_value,
                                   WebApplicationInfo* web_app,
                                   string16* error) {
  DCHECK(web_app->manifest_url.is_valid());

  int error_code = 0;
  std::string error_message;
  scoped_ptr<Value> schema(
      base::JSONReader::ReadAndReturnError(
          ResourceBundle::GetSharedInstance().GetRawDataResource(
              IDR_WEB_APP_SCHEMA),
          base::JSON_PARSE_RFC,  // options
          &error_code,
          &error_message));
  DCHECK(schema.get())
      << "Error parsing JSON schema: " << error_code << ": " << error_message;
  DCHECK(schema->IsType(Value::TYPE_DICTIONARY))
      << "schema root must be dictionary.";

  JSONSchemaValidator validator(static_cast<DictionaryValue*>(schema.get()));

  // We allow extra properties in the schema for easy compat with other systems,
  // and for forward compat with ourselves.
  validator.set_default_allow_additional_properties(true);

  if (!validator.Validate(definition_value)) {
    *error = UTF8ToUTF16(
        validator.errors()[0].path + ": " + validator.errors()[0].message);
    return false;
  }

  // This must be true because the schema requires the root value to be a
  // dictionary.
  DCHECK(definition_value->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* definition = static_cast<DictionaryValue*>(definition_value);

  // Parse launch URL. It must be a valid URL in the same origin as the
  // manifest.
  std::string app_url_string;
  GURL app_url;
  CHECK(definition->GetString("launch_url", &app_url_string));
  if (!(app_url = web_app->manifest_url.Resolve(app_url_string)).is_valid() ||
      app_url.GetOrigin() != web_app->manifest_url.GetOrigin()) {
    *error = UTF8ToUTF16(WebApplicationInfo::kInvalidLaunchURL);
    return false;
  }

  // Parse out the permissions if present.
  std::vector<std::string> permissions;
  ListValue* permissions_value = NULL;
  if (definition->GetList("permissions", &permissions_value)) {
    for (size_t i = 0; i < permissions_value->GetSize(); ++i) {
      std::string permission;
      CHECK(permissions_value->GetString(i, &permission));
      permissions.push_back(permission);
    }
  }

  // Parse out the URLs if present.
  std::vector<GURL> urls;
  ListValue* urls_value = NULL;
  if (definition->GetList("urls", &urls_value)) {
    for (size_t i = 0; i < urls_value->GetSize(); ++i) {
      std::string url_string;
      GURL url;
      CHECK(urls_value->GetString(i, &url_string));
      if (!(url = web_app->manifest_url.Resolve(url_string)).is_valid() ||
          url.GetOrigin() != web_app->manifest_url.GetOrigin()) {
        *error = UTF8ToUTF16(
            JSONSchemaValidator::FormatErrorMessage(
                WebApplicationInfo::kInvalidURL, base::Uint64ToString(i)));
        return false;
      }
      urls.push_back(url);
    }
  }

  // Parse out the icons if present.
  std::vector<WebApplicationInfo::IconInfo> icons;
  DictionaryValue* icons_value = NULL;
  if (definition->GetDictionary("icons", &icons_value)) {
    for (DictionaryValue::key_iterator iter = icons_value->begin_keys();
         iter != icons_value->end_keys(); ++iter) {
      // Ignore unknown properties. Better for forward compat.
      int size = 0;
      if (!base::StringToInt(*iter, &size) || size < 0 || size > 128)
        continue;

      std::string icon_url_string;
      GURL icon_url;
      if (!icons_value->GetString(*iter, &icon_url_string) ||
          !(icon_url = web_app->manifest_url.Resolve(
              icon_url_string)).is_valid()) {
        *error = UTF8ToUTF16(
            JSONSchemaValidator::FormatErrorMessage(
                WebApplicationInfo::kInvalidIconURL, base::IntToString(size)));
        return false;
      }

      WebApplicationInfo::IconInfo icon;
      icon.url = icon_url;
      icon.width = size;
      icon.height = size;

      icons.push_back(icon);
    }
  }

  // Parse if offline mode is enabled.
  definition->GetBoolean("offline_enabled", &web_app->is_offline_enabled);

  CHECK(definition->GetString("name", &web_app->title));
  definition->GetString("description", &web_app->description);
  definition->GetString("launch_container", &web_app->launch_container);
  web_app->app_url = app_url;
  web_app->urls = urls;
  web_app->permissions = permissions;
  web_app->icons = icons;

  return true;
}

}  // namespace web_apps
