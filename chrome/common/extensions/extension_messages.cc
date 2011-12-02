// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_messages.h"

#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "content/public/common/common_param_traits.h"

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params()
    : location(Extension::INVALID),
      creation_flags(Extension::NO_FLAGS){}

ExtensionMsg_Loaded_Params::~ExtensionMsg_Loaded_Params() {}

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params(
    const ExtensionMsg_Loaded_Params& other)
    : manifest(other.manifest),
      location(other.location),
      path(other.path),
      apis(other.apis),
      explicit_hosts(other.explicit_hosts),
      scriptable_hosts(other.scriptable_hosts),
      id(other.id),
      creation_flags(other.creation_flags) {}

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params(
    const Extension* extension)
    : manifest(new DictionaryValue()),
      location(extension->location()),
      path(extension->path()),
      apis(extension->GetActivePermissions()->apis()),
      explicit_hosts(extension->GetActivePermissions()->explicit_hosts()),
      scriptable_hosts(extension->GetActivePermissions()->scriptable_hosts()),
      id(extension->id()),
      creation_flags(extension->creation_flags()) {
  // As we need more bits of extension data in the renderer, add more keys to
  // this list.
  const char* kRendererExtensionKeys[] = {
    extension_manifest_keys::kApp,
    extension_manifest_keys::kContentScripts,
    extension_manifest_keys::kIcons,
    extension_manifest_keys::kName,
    extension_manifest_keys::kPageAction,
    extension_manifest_keys::kPageActions,
    extension_manifest_keys::kPermissions,
    extension_manifest_keys::kPlatformApp,
    extension_manifest_keys::kPublicKey,
    extension_manifest_keys::kVersion,
  };

  // Copy only the data we need and bypass the manifest type checks.
  DictionaryValue* source = extension->manifest()->value();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRendererExtensionKeys); ++i) {
    Value* temp = NULL;
    if (source->Get(kRendererExtensionKeys[i], &temp))
      manifest->Set(kRendererExtensionKeys[i], temp->DeepCopy());
  }
}

scoped_refptr<Extension>
    ExtensionMsg_Loaded_Params::ConvertToExtension() const {
  std::string error;

  scoped_refptr<Extension> extension(
      Extension::Create(path, location, *manifest, creation_flags,
                        &error));
  if (!extension.get())
    DLOG(ERROR) << "Error deserializing extension: " << error;
  else
    extension->SetActivePermissions(
        new ExtensionPermissionSet(apis, explicit_hosts, scriptable_hosts));

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

  // TODO(jstritar): We don't want the URLPattern to fail parsing when the
  // scheme is invalid. Instead, the pattern should parse but it should not
  // match the invalid patterns. We get around this by setting the valid
  // schemes after parsing the pattern. Update these method calls once we can
  // ignore scheme validation with URLPattern parse options. crbug.com/90544
  p->SetValidSchemes(URLPattern::SCHEME_ALL);
  URLPattern::ParseResult result = p->Parse(spec, URLPattern::IGNORE_PORTS);
  p->SetValidSchemes(valid_schemes);
  return URLPattern::PARSE_SUCCESS == result;
}

void ParamTraits<URLPattern>::Log(const param_type& p, std::string* l) {
  LogParam(p.GetAsString(), l);
}

void ParamTraits<URLPatternSet>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.patterns());
}

bool ParamTraits<URLPatternSet>::Read(const Message* m, void** iter,
                                        param_type* p) {
  std::set<URLPattern> patterns;
  if (!ReadParam(m, iter, &patterns))
    return false;

  for (std::set<URLPattern>::iterator i = patterns.begin();
       i != patterns.end(); ++i)
    p->AddPattern(*i);
  return true;
}

void ParamTraits<URLPatternSet>::Log(const param_type& p, std::string* l) {
  LogParam(p.patterns(), l);
}

void ParamTraits<ExtensionAPIPermission::ID>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p));
}

bool ParamTraits<ExtensionAPIPermission::ID>::Read(
    const Message* m, void** iter, param_type* p) {
  int api_id = -2;
  if (!ReadParam(m, iter, &api_id))
    return false;

  *p = static_cast<ExtensionAPIPermission::ID>(api_id);
  return true;
}

void ParamTraits<ExtensionAPIPermission::ID>::Log(
    const param_type& p, std::string* l) {
  LogParam(static_cast<int>(p), l);
}

void ParamTraits<ExtensionMsg_Loaded_Params>::Write(Message* m,
                                                    const param_type& p) {
  WriteParam(m, p.location);
  WriteParam(m, p.path);
  WriteParam(m, *(p.manifest));
  WriteParam(m, p.creation_flags);
  WriteParam(m, p.apis);
  WriteParam(m, p.explicit_hosts);
  WriteParam(m, p.scriptable_hosts);
}

bool ParamTraits<ExtensionMsg_Loaded_Params>::Read(const Message* m,
                                                   void** iter,
                                                   param_type* p) {
  p->manifest.reset(new DictionaryValue());
  return ReadParam(m, iter, &p->location) &&
         ReadParam(m, iter, &p->path) &&
         ReadParam(m, iter, p->manifest.get()) &&
         ReadParam(m, iter, &p->creation_flags) &&
         ReadParam(m, iter, &p->apis) &&
         ReadParam(m, iter, &p->explicit_hosts) &&
         ReadParam(m, iter, &p->scriptable_hosts);
}

void ParamTraits<ExtensionMsg_Loaded_Params>::Log(const param_type& p,
                                                  std::string* l) {
  l->append(p.id);
}

}  // namespace IPC
