// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_messages.h"

#include "chrome/common/extensions/extension_constants.h"
#include "content/common/common_param_traits.h"

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params()
    : location(Extension::INVALID) {
}

ExtensionMsg_Loaded_Params::~ExtensionMsg_Loaded_Params() {
}

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params(
    const ExtensionMsg_Loaded_Params& other)
    : manifest(other.manifest->DeepCopy()),
      location(other.location),
      path(other.path),
      id(other.id) {
}

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params(
    const Extension* extension)
    : manifest(new DictionaryValue()),
      location(extension->location()),
      path(extension->path()),
      id(extension->id()) {
  // As we need more bits of extension data in the renderer, add more keys to
  // this list.
  const char* kRendererExtensionKeys[] = {
    extension_manifest_keys::kPublicKey,
    extension_manifest_keys::kName,
    extension_manifest_keys::kVersion,
    extension_manifest_keys::kIcons,
    extension_manifest_keys::kPermissions,
    extension_manifest_keys::kApp
  };

  // Copy only the data we need.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRendererExtensionKeys); ++i) {
    Value* temp = NULL;
    if (extension->manifest_value()->Get(kRendererExtensionKeys[i], &temp))
      manifest->Set(kRendererExtensionKeys[i], temp->DeepCopy());
  }
}

scoped_refptr<Extension>
    ExtensionMsg_Loaded_Params::ConvertToExtension() const {
  std::string error;

  scoped_refptr<Extension> extension(
      Extension::Create(path, location, *manifest, Extension::NO_FLAGS,
                        &error));
  if (!extension.get())
    LOG(ERROR) << "Error deserializing extension: " << error;

  return extension;
}

namespace IPC {

template <>
struct ParamTraits<Extension::Location> {
  typedef Extension::Location param_type;
  static void Write(Message* m, const param_type& p) {
    int val = static_cast<int>(p);
    WriteParam(m, val);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int val = 0;
    if (!ReadParam(m, iter, &val) ||
        val < Extension::INVALID ||
        val >= Extension::NUM_LOCATIONS)
      return false;
    *p = static_cast<param_type>(val);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    ParamTraits<int>::Log(static_cast<int>(p), l);
  }
};

void ParamTraits<URLPattern>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.valid_schemes());
  WriteParam(m, p.GetAsString());
}

bool ParamTraits<URLPattern>::Read(const Message* m, void** iter,
                                   param_type* p) {
  int valid_schemes;
  std::string spec;
  if (!ReadParam(m, iter, &valid_schemes) ||
      !ReadParam(m, iter, &spec))
    return false;

  p->set_valid_schemes(valid_schemes);
  return URLPattern::PARSE_SUCCESS == p->Parse(spec, URLPattern::PARSE_LENIENT);
}

void ParamTraits<URLPattern>::Log(const param_type& p, std::string* l) {
  LogParam(p.GetAsString(), l);
}

void ParamTraits<ExtensionExtent>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.patterns());
}

bool ParamTraits<ExtensionExtent>::Read(const Message* m, void** iter,
                                        param_type* p) {
  std::vector<URLPattern> patterns;
  bool success =
      ReadParam(m, iter, &patterns);
  if (!success)
    return false;

  for (size_t i = 0; i < patterns.size(); ++i)
    p->AddPattern(patterns[i]);
  return true;
}

void ParamTraits<ExtensionExtent>::Log(const param_type& p, std::string* l) {
  LogParam(p.patterns(), l);
}

void ParamTraits<ExtensionMsg_Loaded_Params>::Write(Message* m,
                                                    const param_type& p) {
  WriteParam(m, p.location);
  WriteParam(m, p.path);
  WriteParam(m, *(p.manifest));
}

bool ParamTraits<ExtensionMsg_Loaded_Params>::Read(const Message* m,
                                                   void** iter,
                                                   param_type* p) {
  p->manifest.reset(new DictionaryValue());
  return ReadParam(m, iter, &p->location) &&
         ReadParam(m, iter, &p->path) &&
         ReadParam(m, iter, p->manifest.get());
}

void ParamTraits<ExtensionMsg_Loaded_Params>::Log(const param_type& p,
                                                  std::string* l) {
  l->append(p.id);
}

}  // namespace IPC
