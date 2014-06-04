// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_JSON_MANIFEST_H
#define COMPONENTS_NACL_RENDERER_JSON_MANIFEST_H

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "third_party/jsoncpp/source/include/json/value.h"

namespace nacl {
class JsonManifest;
class NexeLoadManager;

// There is at most one JsonManifest per PP_Instance. This adds a one-to-one
// mapping.
void AddJsonManifest(PP_Instance instance, scoped_ptr<JsonManifest> manifest);

// Returns a non-owning pointer to the JsonManifest for the given instance.
// Returns NULL if no such JsonManifest exists.
JsonManifest* GetJsonManifest(PP_Instance instance);
void DeleteJsonManifest(PP_Instance instance);

class JsonManifest {
 public:
  struct ErrorInfo {
    PP_NaClError error;
    std::string string;
  };

  JsonManifest(const std::string& manifest_base_url,
               const std::string& sandbox_isa,
               bool nonsfi_enabled,
               bool pnacl_debug);

  // Initialize the manifest object for use by later lookups. Returns
  // true if the manifest parses correctly and matches the schema.
  bool Init(const std::string& json_manifest, ErrorInfo* error_info);

  // Gets the full program URL for the current sandbox ISA from the
  // manifest file.
  bool GetProgramURL(std::string* full_url,
                     PP_PNaClOptions* pnacl_options,
                     bool* uses_nonsfi_mode,
                     ErrorInfo* error_info) const;

  // Resolves a key from the "files" section to a fully resolved URL,
  // i.e., relative URL values are fully expanded relative to the
  // manifest's URL (via ResolveURL).
  // If there was an error, details are reported via error_info.
  bool ResolveKey(const std::string& key,
                  std::string* full_url,
                  PP_PNaClOptions* pnacl_options) const;

 private:
  bool MatchesSchema(ErrorInfo* error_info);
  bool GetKeyUrl(const Json::Value& dictionary,
                 const std::string& key,
                 std::string* full_url,
                 PP_PNaClOptions* pnacl_options) const;
  bool GetURLFromISADictionary(const Json::Value& dictionary,
                               const std::string& parent_key,
                               std::string* url,
                               PP_PNaClOptions* pnacl_options,
                               bool* uses_nonsfi_mode,
                               ErrorInfo* error_info) const;

  std::string manifest_base_url_;
  std::string sandbox_isa_;
  bool nonsfi_enabled_;
  bool pnacl_debug_;

  // The dictionary of manifest information parsed in Init().
  Json::Value dictionary_;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_JSON_MANIFEST_H
