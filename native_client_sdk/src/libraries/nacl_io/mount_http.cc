// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_http.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <vector>

#include <ppapi/c/pp_errors.h>

#include "nacl_io/mount_node_dir.h"
#include "nacl_io/mount_node_http.h"
#include "nacl_io/osinttypes.h"
#include "nacl_io/osunistd.h"

namespace nacl_io {

namespace {

typedef std::vector<char*> StringList_t;
size_t SplitString(char* str, const char* delim, StringList_t* list) {
  char* item = strtok(str, delim);

  list->clear();
  while (item) {
    list->push_back(item);
    item = strtok(NULL, delim);
  }

  return list->size();
}

}  // namespace

std::string NormalizeHeaderKey(const std::string& s) {
  // Capitalize the first letter and any letter following a hyphen:
  // e.g. ACCEPT-ENCODING -> Accept-Encoding
  std::string result;
  bool upper = true;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    result += upper ? toupper(c) : tolower(c);
    upper = c == '-';
  }

  return result;
}

Error MountHttp::Access(const Path& path, int a_mode) {
  assert(url_root_.empty() || url_root_[url_root_.length() - 1] == '/');

  NodeMap_t::iterator iter = node_cache_.find(path.Join());
  if (iter == node_cache_.end()) {
    // If we can't find the node in the cache, fetch it
    std::string url = MakeUrl(path);
    ScopedMountNode node(new MountNodeHttp(this, url, cache_content_));
    Error error = node->Init(O_RDONLY);
    if (error)
      return error;

    error = node->GetStat(NULL);
    if (error)
      return error;
  }

  // Don't allow write or execute access.
  if (a_mode & (W_OK | X_OK))
    return EACCES;

  return 0;
}

Error MountHttp::Open(const Path& path, int mode, ScopedMountNode* out_node) {
  out_node->reset(NULL);
  assert(url_root_.empty() || url_root_[url_root_.length() - 1] == '/');

  NodeMap_t::iterator iter = node_cache_.find(path.Join());
  if (iter != node_cache_.end()) {
    *out_node = iter->second;
    return 0;
  }

  // If we can't find the node in the cache, create it
  std::string url = MakeUrl(path);
  ScopedMountNode node(new MountNodeHttp(this, url, cache_content_));
  Error error = node->Init(mode);
  if (error)
    return error;

  error = node->GetStat(NULL);
  if (error)
    return error;

  ScopedMountNode parent;
  error = FindOrCreateDir(path.Parent(), &parent);
  if (error)
    return error;

  error = parent->AddChild(path.Basename(), node);
  if (error)
    return error;

  node_cache_[path.Join()] = node;
  *out_node = node;
  return 0;
}

Error MountHttp::Unlink(const Path& path) {
  NodeMap_t::iterator iter = node_cache_.find(path.Join());
  if (iter == node_cache_.end())
    return ENOENT;

  if (iter->second->IsaDir())
    return EISDIR;

  return EACCES;
}

Error MountHttp::Mkdir(const Path& path, int permissions) {
  NodeMap_t::iterator iter = node_cache_.find(path.Join());
  if (iter != node_cache_.end()) {
    if (iter->second->IsaDir())
      return EEXIST;
  }
  return EACCES;
}

Error MountHttp::Rmdir(const Path& path) {
  NodeMap_t::iterator iter = node_cache_.find(path.Join());
  if (iter == node_cache_.end())
    return ENOENT;

  if (!iter->second->IsaDir())
    return ENOTDIR;

  return EACCES;
}

Error MountHttp::Remove(const Path& path) {
  NodeMap_t::iterator iter = node_cache_.find(path.Join());
  if (iter == node_cache_.end())
    return ENOENT;

  return EACCES;
}

PP_Resource MountHttp::MakeUrlRequestInfo(const std::string& url,
                                          const char* method,
                                          StringMap_t* additional_headers) {
  URLRequestInfoInterface* interface = ppapi_->GetURLRequestInfoInterface();
  VarInterface* var_interface = ppapi_->GetVarInterface();

  PP_Resource request_info = interface->Create(ppapi_->GetInstance());
  if (!request_info)
    return 0;

  interface->SetProperty(request_info,
                         PP_URLREQUESTPROPERTY_URL,
                         var_interface->VarFromUtf8(url.c_str(), url.length()));
  interface->SetProperty(request_info,
                         PP_URLREQUESTPROPERTY_METHOD,
                         var_interface->VarFromUtf8(method, strlen(method)));
  interface->SetProperty(request_info,
                         PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS,
                         PP_MakeBool(allow_cors_ ? PP_TRUE : PP_FALSE));
  interface->SetProperty(request_info,
                         PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS,
                         PP_MakeBool(allow_credentials_ ? PP_TRUE : PP_FALSE));

  // Merge the mount headers with the request headers. If the field is already
  // set it |additional_headers|, don't use the one from headers_.
  for (StringMap_t::iterator iter = headers_.begin(); iter != headers_.end();
       ++iter) {
    const std::string& key = NormalizeHeaderKey(iter->first);
    if (additional_headers->find(key) == additional_headers->end()) {
      additional_headers->insert(std::make_pair(key, iter->second));
    }
  }

  // Join the headers into one string.
  std::string headers;
  for (StringMap_t::iterator iter = additional_headers->begin();
       iter != additional_headers->end();
       ++iter) {
    headers += iter->first + ": " + iter->second + '\n';
  }

  interface->SetProperty(
      request_info,
      PP_URLREQUESTPROPERTY_HEADERS,
      var_interface->VarFromUtf8(headers.c_str(), headers.length()));

  return request_info;
}

MountHttp::MountHttp()
    : allow_cors_(false),
      allow_credentials_(false),
      cache_stat_(true),
      cache_content_(true) {}

Error MountHttp::Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
  Error error = Mount::Init(dev, args, ppapi);
  if (error)
    return error;

  // Parse mount args.
  for (StringMap_t::iterator iter = args.begin(); iter != args.end(); ++iter) {
    if (iter->first == "SOURCE") {
      url_root_ = iter->second;

      // Make sure url_root_ ends with a slash.
      if (!url_root_.empty() && url_root_[url_root_.length() - 1] != '/') {
        url_root_ += '/';
      }
    } else if (iter->first == "manifest") {
      char* text;
      error = LoadManifest(iter->second, &text);
      if (error)
        return error;

      error = ParseManifest(text);
      if (error) {
        delete[] text;
        return error;
      }

      delete[] text;
    } else if (iter->first == "allow_cross_origin_requests") {
      allow_cors_ = iter->second == "true";
    } else if (iter->first == "allow_credentials") {
      allow_credentials_ = iter->second == "true";
    } else if (iter->first == "cache_stat") {
      cache_stat_ = iter->second == "true";
    } else if (iter->first == "cache_content") {
      cache_content_ = iter->second == "true";
    } else {
      // Assume it is a header to pass to an HTTP request.
      headers_[NormalizeHeaderKey(iter->first)] = iter->second;
    }
  }

  return 0;
}

void MountHttp::Destroy() {}

Error MountHttp::FindOrCreateDir(const Path& path,
                                 ScopedMountNode* out_node) {
  out_node->reset(NULL);
  std::string strpath = path.Join();
  NodeMap_t::iterator iter = node_cache_.find(strpath);
  if (iter != node_cache_.end()) {
    *out_node = iter->second;
    return 0;
  }

  // If the node does not exist, create it.
  ScopedMountNode node(new MountNodeDir(this));
  Error error = node->Init(S_IREAD);
  if (error)
    return error;

  // If not the root node, find the parent node and add it to the parent
  if (!path.Top()) {
    ScopedMountNode parent;
    error = FindOrCreateDir(path.Parent(), &parent);
    if (error)
      return error;

    error = parent->AddChild(path.Basename(), node);
    if (error)
      return error;
  }

  // Add it to the node cache.
  node_cache_[strpath] = node;
  *out_node = node;
  return 0;
}

Error MountHttp::ParseManifest(char* text) {
  StringList_t lines;
  SplitString(text, "\n", &lines);

  for (size_t i = 0; i < lines.size(); i++) {
    StringList_t words;
    SplitString(lines[i], " ", &words);

    if (words.size() == 3) {
      char* modestr = words[0];
      char* lenstr = words[1];
      char* name = words[2];

      assert(modestr && strlen(modestr) == 4);
      assert(name && name[0] == '/');
      assert(lenstr);

      // Only support regular and streams for now
      // Ignore EXEC bit
      int mode = S_IFREG;
      switch (modestr[0]) {
        case '-':
          mode = S_IFREG;
          break;
        case 'c':
          mode = S_IFCHR;
          break;
        default:
          fprintf(stderr, "Unable to parse type %s for %s.\n", modestr, name);
          return EINVAL;
      }

      switch (modestr[1]) {
        case '-':
          break;
        case 'r':
          mode |= S_IREAD;
          break;
        default:
          fprintf(stderr, "Unable to parse read %s for %s.\n", modestr, name);
          return EINVAL;
      }

      switch (modestr[2]) {
        case '-':
          break;
        case 'w':
          mode |= S_IWRITE;
          break;
        default:
          fprintf(stderr, "Unable to parse write %s for %s.\n", modestr, name);
          return EINVAL;
      }

      Path path(name);
      std::string url = MakeUrl(path);

      MountNodeHttp* http_node = new MountNodeHttp(this, url, cache_content_);
      ScopedMountNode node(http_node);

      Error error = node->Init(mode);
      if (error)
        return error;
      http_node->SetCachedSize(atoi(lenstr));

      ScopedMountNode dir_node;
      error = FindOrCreateDir(path.Parent(), &dir_node);
      if (error)
        return error;

      error = dir_node->AddChild(path.Basename(), node);
      if (error)
        return error;

      std::string pname = path.Join();
      node_cache_[pname] = node;
    }
  }

  return 0;
}

Error MountHttp::LoadManifest(const std::string& manifest_name,
                              char** out_manifest) {
  Path manifest_path(manifest_name);
  ScopedMountNode manifest_node;
  *out_manifest = NULL;

  int error = Open(manifest_path, O_RDONLY, &manifest_node);
  if (error)
    return error;

  size_t size;
  error = manifest_node->GetSize(&size);
  if (error)
    return error;

  char* text = new char[size + 1];
  int len;
  error = manifest_node->Read(0, text, size, &len);
  if (error)
    return error;

  text[len] = 0;
  *out_manifest = text;
  return 0;
}

std::string MountHttp::MakeUrl(const Path& path) {
  return url_root_ +
         (path.IsAbsolute() ? path.Range(1, path.Size()) : path.Join());
}

}  // namespace nacl_io
