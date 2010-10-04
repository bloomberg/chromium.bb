// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/policy_settings.h"

#include "base/logging.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/policy_constants.h"
#include "chrome_frame/utils.h"

PolicySettings::RendererForUrl PolicySettings::GetRendererForUrl(
    const wchar_t* url) {
  RendererForUrl renderer = default_renderer_;
  std::vector<std::wstring>::const_iterator it;
  for (it = renderer_exclusion_list_.begin();
       it != renderer_exclusion_list_.end(); ++it) {
    if (MatchPattern(url, (*it))) {
      renderer = (renderer == RENDER_IN_HOST) ?
          RENDER_IN_CHROME_FRAME : RENDER_IN_HOST;
      break;
    }
  }
  return renderer;
}

PolicySettings::RendererForUrl PolicySettings::GetRendererForContentType(
    const wchar_t* content_type) {
  DCHECK(content_type);
  RendererForUrl renderer = RENDERER_NOT_SPECIFIED;
  std::vector<std::wstring>::const_iterator it;
  for (it = content_type_list_.begin();
       it != content_type_list_.end(); ++it) {
    if (lstrcmpiW(content_type, (*it).c_str()) == 0) {
      renderer = RENDER_IN_CHROME_FRAME;
      break;
    }
  }
  return renderer;
}

void PolicySettings::RefreshFromRegistry() {
  default_renderer_ = RENDERER_NOT_SPECIFIED;
  renderer_exclusion_list_.clear();

  RegKey config_key;
  DWORD value = RENDERER_NOT_SPECIFIED;
  HKEY root_key[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
  std::wstring settings_value(
      ASCIIToWide(policy::key::kChromeFrameRendererSettings));
  for (int i = 0; i < arraysize(root_key); ++i) {
    if (config_key.Open(root_key[i], policy::kRegistrySubKey, KEY_READ) &&
        config_key.ReadValueDW(settings_value.c_str(), &value)) {
      break;
    } else {
      config_key.Close();
    }
  }

  DCHECK(value == RENDERER_NOT_SPECIFIED ||
         value == RENDER_IN_HOST ||
         value == RENDER_IN_CHROME_FRAME) <<
      "invalid default renderer setting: " << value;

  if (value != RENDER_IN_HOST && value != RENDER_IN_CHROME_FRAME) {
    DLOG(INFO) << "default renderer not specified via policy";
  } else {
    default_renderer_ = static_cast<RendererForUrl>(value);
    const char* exclusion_list_name = (default_renderer_ == RENDER_IN_HOST) ?
        policy::key::kRenderInChromeFrameList :
        policy::key::kRenderInHostList;

    EnumerateKeyValues(config_key.Handle(),
        ASCIIToWide(exclusion_list_name).c_str(), &renderer_exclusion_list_);

    DLOG(INFO) << "Default renderer as specified via policy: " <<
        default_renderer_ << " exclusion list size: " <<
        renderer_exclusion_list_.size();
  }

  std::wstring sub_key(policy::kRegistrySubKey);
  sub_key += L"\\";
  sub_key += ASCIIToWide(policy::key::kChromeFrameContentTypes);

  for (int i = 0; i < arraysize(root_key) && content_type_list_.size() == 0;
       ++i) {
    EnumerateKeyValues(root_key[i], sub_key.c_str(), &content_type_list_);
  }
}

