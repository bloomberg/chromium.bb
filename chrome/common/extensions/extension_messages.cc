// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_messages.h"

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "content/public/common/common_param_traits.h"

using extensions::APIPermission;
using extensions::APIPermissionInfo;
using extensions::APIPermissionMap;
using extensions::APIPermissionSet;
using extensions::Extension;
using extensions::MediaGalleriesPermission;
using extensions::PermissionSet;
using extensions::SocketPermissionData;

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params()
    : location(Extension::INVALID),
      creation_flags(Extension::NO_FLAGS){}

ExtensionMsg_Loaded_Params::~ExtensionMsg_Loaded_Params() {}

ExtensionMsg_Loaded_Params::ExtensionMsg_Loaded_Params(
    const Extension* extension)
    : manifest(extension->manifest()->value()->DeepCopy()),
      location(extension->location()),
      path(extension->path()),
      apis(extension->GetActivePermissions()->apis()),
      explicit_hosts(extension->GetActivePermissions()->explicit_hosts()),
      scriptable_hosts(extension->GetActivePermissions()->scriptable_hosts()),
      id(extension->id()),
      creation_flags(extension->creation_flags()) {
}

scoped_refptr<Extension>
    ExtensionMsg_Loaded_Params::ConvertToExtension() const {
  std::string error;

  scoped_refptr<Extension> extension(
      Extension::Create(path, location, *manifest, creation_flags,
                        &error));
  if (!extension.get()) {
    DLOG(ERROR) << "Error deserializing extension: " << error;
    return extension;
  }

  extension->SetActivePermissions(
        new PermissionSet(apis, explicit_hosts, scriptable_hosts));

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
  static bool Read(const Message* m, PickleIterator* iter, param_type* p) {
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

bool ParamTraits<URLPattern>::Read(const Message* m, PickleIterator* iter,
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
  URLPattern::ParseResult result = p->Parse(spec);
  p->SetValidSchemes(valid_schemes);
  return URLPattern::PARSE_SUCCESS == result;
}

void ParamTraits<URLPattern>::Log(const param_type& p, std::string* l) {
  LogParam(p.GetAsString(), l);
}

void ParamTraits<URLPatternSet>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.patterns());
}

bool ParamTraits<URLPatternSet>::Read(const Message* m, PickleIterator* iter,
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

void ParamTraits<APIPermission::ID>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p));
}

bool ParamTraits<APIPermission::ID>::Read(
    const Message* m, PickleIterator* iter, param_type* p) {
  int api_id = -2;
  if (!ReadParam(m, iter, &api_id))
    return false;

  *p = static_cast<APIPermission::ID>(api_id);
  return true;
}

void ParamTraits<APIPermission::ID>::Log(
    const param_type& p, std::string* l) {
  LogParam(static_cast<int>(p), l);
}

void ParamTraits<APIPermission*>::Log(
    const param_type& p, std::string* l) {
  p->Log(l);
}

void ParamTraits<APIPermissionSet>::Write(
    Message* m, const param_type& p) {
  APIPermissionSet::const_iterator it = p.begin();
  const APIPermissionSet::const_iterator end = p.end();
  WriteParam(m, p.size());
  for (; it != end; ++it) {
    WriteParam(m, it->id());
    it->Write(m);
  }
}

bool ParamTraits<APIPermissionSet>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  size_t size;
  if (!ReadParam(m, iter, &size))
    return false;
  for (size_t i = 0; i < size; ++i) {
    APIPermission::ID id;
    if (!ReadParam(m, iter, &id))
      return false;
    const APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(id);
    if (!permission_info)
      return false;
    scoped_ptr<APIPermission> p(permission_info->CreateAPIPermission());
    if (!p->Read(m, iter))
      return false;
    r->insert(p.release());
  }
  return true;
}

void ParamTraits<APIPermissionSet>::Log(
    const param_type& p, std::string* l) {
  LogParam(p.map(), l);
}

void ParamTraits<SocketPermissionData>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.GetAsString());
}

bool ParamTraits<SocketPermissionData>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  std::string spec;
  if (!ReadParam(m, iter, &spec))
    return false;

  return r->Parse(spec);
}

void ParamTraits<SocketPermissionData>::Log(
    const param_type& p, std::string* l) {
  LogParam(std::string("<SocketPermissionData>"), l);
}

void ParamTraits<MediaGalleriesPermission::PermissionTypes>::Write(
    Message* m, const param_type& p) {
  WriteParam(m,
             std::string(MediaGalleriesPermission::PermissionTypeToString(p)));
}

bool ParamTraits<MediaGalleriesPermission::PermissionTypes>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  std::string permission;
  if (!ReadParam(m, iter, &permission))
    return false;
  *r = MediaGalleriesPermission::PermissionStringToType(permission);

  return *r != MediaGalleriesPermission::kNone;
}

void ParamTraits<MediaGalleriesPermission::PermissionTypes>::Log(
    const param_type& p, std::string* l) {
  LogParam(std::string("<MediaGalleriesPermission::PermissionTypes>"), l);
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
                                                   PickleIterator* iter,
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
