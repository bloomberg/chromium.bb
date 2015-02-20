// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEM_INFO_H_
#define MEDIA_BASE_KEY_SYSTEM_INFO_H_

#include <string>

#include "media/base/eme_constants.h"
#include "media/base/media_export.h"

// Definitions:
// * Key system
//    https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#key-system
// * Concrete key system
//    A key system string that can be instantiated, such as
//    via the MediaKeys constructor. Examples include "org.w3.clearkey" and
//    "com.widevine.alpha".
// * Abstract key system
//    A key system string that cannot be instantiated like a concrete key system
//    but is otherwise useful, such as in discovery using isTypeSupported().
// * Parent key system
//    A key system string that is one level up from the child key system. It may
//    be an abstract key system.
//    As an example, "com.example" is the parent of "com.example.foo".

namespace media {

// Contains information about an EME key system as well as how to instantiate
// the corresponding CDM.
struct MEDIA_EXPORT KeySystemInfo {
  KeySystemInfo();
  ~KeySystemInfo();

  std::string key_system;

  // Specifies registered initialization data types supported by |key_system|.
  SupportedInitDataTypes supported_init_data_types;

  // Specifies codecs supported by |key_system|.
  SupportedCodecs supported_codecs;

  // Specifies session types and features supported by |key_system|.
  EmeSessionTypeSupport persistent_license_support;
  EmeSessionTypeSupport persistent_release_message_support;
  EmeFeatureSupport persistent_state_support;
  EmeFeatureSupport distinctive_identifier_support;

  // A hierarchical parent for |key_system|. This value can be used to check
  // supported types but cannot be used to instantiate a MediaKeys object.
  // Only one parent key system is currently supported per concrete key system.
  std::string parent_key_system;

  // The following indicate how the corresponding CDM should be instantiated.
  bool use_aes_decryptor;
#if defined(ENABLE_PEPPER_CDMS)
  std::string pepper_type;
#endif
};

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEM_INFO_H_
