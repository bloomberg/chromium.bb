/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nacl_io/mount_http.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ppapi/c/pp_errors.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include "nacl_io/mount_node_dir.h"
#include "nacl_io/osinttypes.h"
#include "utils/auto_lock.h"

#if defined(WIN32)
#define snprintf _snprintf
#endif


namespace {

typedef std::vector<char *> StringList_t;
size_t SplitString(char *str, const char *delim, StringList_t* list) {
  char *item = strtok(str, delim);

  list->clear();
  while (item) {
    list->push_back(item);
    item = strtok(NULL, delim);
  }

  return list->size();
}


// If we're attempting to read a partial request, but the server returns a full
// request, we need to read all of the data up to the start of our partial
// request into a dummy buffer. This is the maximum size of that buffer.
const size_t MAX_READ_BUFFER_SIZE = 64 * 1024;
const int32_t STATUSCODE_OK = 200;
const int32_t STATUSCODE_PARTIAL_CONTENT = 206;

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

StringMap_t ParseHeaders(const char* headers, int32_t headers_length) {
  enum State {
    FINDING_KEY,
    SKIPPING_WHITESPACE,
    FINDING_VALUE,
  };

  StringMap_t result;
  std::string key;
  std::string value;

  State state = FINDING_KEY;
  const char* start = headers;
  for (int i = 0; i < headers_length; ++i) {
    switch (state) {
      case FINDING_KEY:
        if (headers[i] == ':') {
          // Found key.
          key.assign(start, &headers[i] - start);
          key = NormalizeHeaderKey(key);
          state = SKIPPING_WHITESPACE;
        }
        break;

      case SKIPPING_WHITESPACE:
        if (headers[i] == ' ') {
          // Found whitespace, keep going...
          break;
        }

        // Found a non-whitespace, mark this as the start of the value.
        start = &headers[i];
        state = FINDING_VALUE;
        // Fallthrough to start processing value without incrementing i.

      case FINDING_VALUE:
        if (headers[i] == '\n') {
          // Found value.
          value.assign(start, &headers[i] - start);
          result[key] = value;
          start = &headers[i + 1];
          state = FINDING_KEY;
        }
        break;
    }
  }

  return result;
}

bool ParseContentLength(const StringMap_t& headers, size_t* content_length) {
  StringMap_t::const_iterator iter = headers.find("Content-Length");
  if (iter == headers.end())
    return false;

  *content_length = strtoul(iter->second.c_str(), NULL, 10);
  return true;
}

bool ParseContentRange(const StringMap_t& headers, size_t* read_start,
                       size_t* read_end, size_t* entity_length) {
  StringMap_t::const_iterator iter = headers.find("Content-Range");
  if (iter == headers.end())
    return false;

  // The key should look like "bytes ##-##/##" or "bytes ##-##/*". The last
  // value is the entity length, which can potentially be * (i.e. unknown).
  int read_start_int;
  int read_end_int;
  int entity_length_int;
  int result = sscanf(iter->second.c_str(), "bytes %"SCNuS"-%"SCNuS"/%"SCNuS,
                      &read_start_int, &read_end_int, &entity_length_int);

  // The Content-Range header specifies an inclusive range: e.g. the first ten
  // bytes is "bytes 0-9/*". Convert it to a half-open range by incrementing
  // read_end.
  if (result == 2) {
    *read_start = read_start_int;
    *read_end = read_end_int + 1;
    *entity_length = 0;
    return true;
  } else if (result == 3) {
    *read_start = read_start_int;
    *read_end = read_end_int + 1;
    *entity_length = entity_length_int;
    return true;
  }

  return false;
}

}  // namespace


class MountNodeHttp : public MountNode {
 public:
  virtual int FSync();
  virtual int GetDents(size_t offs, struct dirent* pdir, size_t count);
  virtual int GetStat(struct stat* stat);
  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Truncate(size_t size);
  virtual int Write(size_t offs, const void* buf, size_t count);
  virtual size_t GetSize();

  void SetCachedSize(off_t size);

 protected:
  MountNodeHttp(Mount* mount, const std::string& url, bool cache_content);

 private:
  bool OpenUrl(const char* method,
               StringMap_t* request_headers,
               PP_Resource* out_loader,
               PP_Resource* out_request,
               PP_Resource* out_response,
               int32_t* out_statuscode,
               StringMap_t* out_response_headers);

  int DownloadToCache();
  int ReadPartialFromCache(size_t offs, void* buf, size_t count);
  int DownloadPartial(size_t offs, void* buf, size_t count);
  int DownloadToBuffer(PP_Resource loader, void* buf, size_t count);

  std::string url_;
  std::vector<char> buffer_;

  bool cache_content_;
  bool has_cached_size_;
  std::vector<char> cached_data_;

  friend class MountHttp;
};

void MountNodeHttp::SetCachedSize(off_t size) {
  has_cached_size_ = true;
  stat_.st_size = size;
}

int MountNodeHttp::FSync() {
  errno = ENOSYS;
  return -1;
}

int MountNodeHttp::GetDents(size_t offs, struct dirent* pdir, size_t count) {
  errno = ENOSYS;
  return -1;
}

int MountNodeHttp::GetStat(struct stat* stat) {
  AutoLock lock(&lock_);

  // Assume we need to 'HEAD' if we do not know the size, otherwise, assume
  // that the information is constant.  We can add a timeout if needed.
  MountHttp* mount = static_cast<MountHttp*>(mount_);
  if (stat_.st_size == 0 || !mount->cache_stat_) {
    StringMap_t headers;
    PP_Resource loader;
    PP_Resource request;
    PP_Resource response;
    int32_t statuscode;
    StringMap_t response_headers;
    if (!OpenUrl("HEAD", &headers, &loader, &request, &response, &statuscode,
                &response_headers)) {
      // errno is already set by OpenUrl.
      return -1;
    }

    ScopedResource scoped_loader(mount_->ppapi(), loader);
    ScopedResource scoped_request(mount_->ppapi(), request);
    ScopedResource scoped_response(mount_->ppapi(), response);


    size_t entity_length;
    if (ParseContentLength(response_headers, &entity_length))
      SetCachedSize(static_cast<off_t>(entity_length));
    else
      // Don't use SetCachedSize here -- it is actually unknown.
      stat_.st_size = 0;

    stat_.st_atime = 0; // TODO(binji): Use "Last-Modified".
    stat_.st_mtime = 0;
    stat_.st_ctime = 0;
  }

  // Fill the stat structure if provided
  if (stat) {
    memcpy(stat, &stat_, sizeof(stat_));
  }
  return 0;
}

int MountNodeHttp::Read(size_t offs, void* buf, size_t count) {
  AutoLock lock(&lock_);
  if (cache_content_) {
    if (cached_data_.empty()) {
      if (DownloadToCache() < 0)
        return -1;
    }

    return ReadPartialFromCache(offs, buf, count);
  }

  return DownloadPartial(offs, buf, count);
}

int MountNodeHttp::Truncate(size_t size) {
  errno = ENOSYS;
  return -1;
}

int MountNodeHttp::Write(size_t offs, const void* buf, size_t count) {
  // TODO(binji): support POST?
  errno = ENOSYS;
  return -1;
}

size_t MountNodeHttp::GetSize() {
  // TODO(binji): This value should be cached properly; i.e. obey the caching
  // headers returned by the server.
  AutoLock lock(&lock_);
  if (!has_cached_size_) {
    // Even if DownloadToCache fails, the best result we can return is what
    // was written to stat_.st_size.
    if (cache_content_)
      DownloadToCache();
  }

  return stat_.st_size;
}

MountNodeHttp::MountNodeHttp(Mount* mount, const std::string& url,
                             bool cache_content)
    : MountNode(mount),
      url_(url),
      cache_content_(cache_content),
      has_cached_size_(false) {
}

bool MountNodeHttp::OpenUrl(const char* method,
                           StringMap_t* request_headers,
                           PP_Resource* out_loader,
                           PP_Resource* out_request,
                           PP_Resource* out_response,
                           int32_t* out_statuscode,
                           StringMap_t* out_response_headers) {
  // Assume lock_ is already held.

  PepperInterface* ppapi = mount_->ppapi();

  MountHttp* mount_http = static_cast<MountHttp*>(mount_);
  ScopedResource request(ppapi,
                         mount_http->MakeUrlRequestInfo(url_, method,
                                                        request_headers));
  if (!request.pp_resource()) {
    errno = EINVAL;
    return false;
  }

  URLLoaderInterface* loader_interface = ppapi->GetURLLoaderInterface();
  URLResponseInfoInterface* response_interface =
      ppapi->GetURLResponseInfoInterface();
  VarInterface* var_interface = ppapi->GetVarInterface();

  ScopedResource loader(ppapi, loader_interface->Create(ppapi->GetInstance()));
  if (!loader.pp_resource()) {
    errno = EINVAL;
    return false;
  }

  int32_t result = loader_interface->Open(
      loader.pp_resource(), request.pp_resource(), PP_BlockUntilComplete());
  if (result != PP_OK) {
    errno = PPErrorToErrno(result);
    return false;
  }

  ScopedResource response(
      ppapi,
      loader_interface->GetResponseInfo(loader.pp_resource()));
  if (!response.pp_resource()) {
    errno = EINVAL;
    return false;
  }

  // Get response statuscode.
  PP_Var statuscode = response_interface->GetProperty(
      response.pp_resource(),
      PP_URLRESPONSEPROPERTY_STATUSCODE);

  if (statuscode.type != PP_VARTYPE_INT32) {
    errno = EINVAL;
    return false;
  }

  *out_statuscode = statuscode.value.as_int;

  // Only accept OK or Partial Content.
  if (*out_statuscode != STATUSCODE_OK &&
      *out_statuscode != STATUSCODE_PARTIAL_CONTENT) {
    errno = EINVAL;
    return false;
  }

  // Get response headers.
  PP_Var response_headers_var = response_interface->GetProperty(
      response.pp_resource(),
      PP_URLRESPONSEPROPERTY_HEADERS);

  uint32_t response_headers_length;
  const char* response_headers_str = var_interface->VarToUtf8(
      response_headers_var,
      &response_headers_length);

  *out_loader = loader.Release();
  *out_request = request.Release();
  *out_response = response.Release();
  *out_response_headers = ParseHeaders(response_headers_str,
                                       response_headers_length);

  return true;
}

int MountNodeHttp::DownloadToCache() {
  StringMap_t headers;
  PP_Resource loader;
  PP_Resource request;
  PP_Resource response;
  int32_t statuscode;
  StringMap_t response_headers;
  if (!OpenUrl("GET", &headers, &loader, &request, &response, &statuscode,
               &response_headers)) {
    // errno is already set by OpenUrl.
    return -1;
  }

  PepperInterface* ppapi = mount_->ppapi();
  ScopedResource scoped_loader(ppapi, loader);
  ScopedResource scoped_request(ppapi, request);
  ScopedResource scoped_response(ppapi, response);

  size_t content_length = 0;
  if (ParseContentLength(response_headers, &content_length)) {
    cached_data_.resize(content_length);
    int real_size = DownloadToBuffer(loader, cached_data_.data(),
                                     content_length);
    if (real_size < 0)
      return -1;

    SetCachedSize(real_size);
    cached_data_.resize(real_size);
    return real_size;
  }

  // We don't know how big the file is. Read in chunks.
  cached_data_.resize(MAX_READ_BUFFER_SIZE);
  char* buf = cached_data_.data();
  size_t total_bytes_read = 0;
  size_t bytes_to_read = MAX_READ_BUFFER_SIZE;
  while (true) {
    int bytes_read = DownloadToBuffer(loader, buf, bytes_to_read);
    if (bytes_read < 0)
      return -1;

    total_bytes_read += bytes_read;

    if (bytes_read < bytes_to_read) {
      SetCachedSize(total_bytes_read);
      cached_data_.resize(total_bytes_read);
      return total_bytes_read;
    }

    buf += bytes_read;
    cached_data_.resize(total_bytes_read + bytes_to_read);
  }
}

int MountNodeHttp::ReadPartialFromCache(size_t offs, void* buf, size_t count) {
  if (offs > cached_data_.size()) {
    errno = EINVAL;
    return -1;
  }

  count = std::min(count, cached_data_.size() - offs);
  memcpy(buf, &cached_data_.data()[offs], count);
  return count;
}

int MountNodeHttp::DownloadPartial(size_t offs, void* buf, size_t count) {
  StringMap_t headers;

  char buffer[100];
  // Range request is inclusive: 0-99 returns 100 bytes.
  snprintf(&buffer[0], sizeof(buffer), "bytes=%"PRIuS"-%"PRIuS,
           offs, offs + count - 1);
  headers["Range"] = buffer;

  PP_Resource loader;
  PP_Resource request;
  PP_Resource response;
  int32_t statuscode;
  StringMap_t response_headers;
  if (!OpenUrl("GET", &headers, &loader, &request, &response, &statuscode,
               &response_headers)) {
    // errno is already set by OpenUrl.
    return -1;
  }

  PepperInterface* ppapi = mount_->ppapi();
  ScopedResource scoped_loader(ppapi, loader);
  ScopedResource scoped_request(ppapi, request);
  ScopedResource scoped_response(ppapi, response);

  size_t read_start = 0;
  if (statuscode == STATUSCODE_OK) {
    // No partial result, read everything starting from the part we care about.
    size_t content_length;
    if (ParseContentLength(response_headers, &content_length)) {
      if (offs >= content_length) {
        errno = EINVAL;
        return 0;
      }

      // Clamp count, if trying to read past the end of the file.
      if (offs + count > content_length) {
        count = content_length - offs;
      }
    }
  } else if (statuscode == STATUSCODE_PARTIAL_CONTENT) {
    // Determine from the headers where we are reading.
    size_t read_end;
    size_t entity_length;
    if (ParseContentRange(response_headers, &read_start, &read_end,
                          &entity_length)) {
      if (read_start > offs || read_start > read_end) {
        // If this error occurs, the server is returning bogus values.
        errno = EINVAL;
        return -1;
      }

      // Clamp count, if trying to read past the end of the file.
      count = std::min(read_end - read_start, count);
    } else {
      // Partial Content without Content-Range. Assume that the server gave us
      // exactly what we asked for. This can happen even when the server
      // returns 200 -- the cache may return 206 in this case, but not modify
      // the headers.
      read_start = offs;
    }
  }

  if (read_start < offs) {
    // We aren't yet at the location where we want to start reading. Read into
    // our dummy buffer until then.
    size_t bytes_to_read = offs - read_start;
    if (buffer_.size() < bytes_to_read)
      buffer_.resize(std::min(bytes_to_read, MAX_READ_BUFFER_SIZE));

    while (bytes_to_read > 0) {
      int32_t bytes_read = DownloadToBuffer(loader, buffer_.data(),
                                            buffer_.size());
      if (bytes_read < 0)
        return -1;

      bytes_to_read -= bytes_read;
    }
  }

  return DownloadToBuffer(loader, buf, count);
}

int MountNodeHttp::DownloadToBuffer(PP_Resource loader, void* buf,
                                    size_t count) {
  PepperInterface* ppapi = mount_->ppapi();
  URLLoaderInterface* loader_interface = ppapi->GetURLLoaderInterface();

  char* out_buffer = static_cast<char*>(buf);
  size_t bytes_to_read = count;
  while (bytes_to_read > 0) {
    int32_t bytes_read = loader_interface->ReadResponseBody(
        loader, out_buffer, bytes_to_read, PP_BlockUntilComplete());

    if (bytes_read == 0) {
      // This is not an error -- it may just be that we were trying to read
      // more data than exists.
      return count - bytes_to_read;
    }

    if (bytes_read < 0) {
      errno = PPErrorToErrno(bytes_read);
      return -1;
    }

    assert(bytes_read <= bytes_to_read);
    bytes_to_read -= bytes_read;
    out_buffer += bytes_read;
  }

  return count;
}

MountNode *MountHttp::Open(const Path& path, int mode) {
  assert(url_root_.empty() || url_root_[url_root_.length() - 1] == '/');

  NodeMap_t::iterator iter = node_cache_.find(path.Join());
  if (iter != node_cache_.end()) {
    return iter->second;
  }

  // If we can't find the node in the cache, create it
  std::string url = url_root_ + (path.IsAbsolute() ?
      path.Range(1, path.Size()) :
      path.Join());

  MountNodeHttp* node = new MountNodeHttp(this, url, cache_content_);
  if (!node->Init(mode) || (0 != node->GetStat(NULL))) {
    node->Release();
    return NULL;
  }

  MountNodeDir* parent = FindOrCreateDir(path.Parent());
  node_cache_[path.Join()] = node;
  parent->AddChild(path.Basename(), node);
  return node;
}

int MountHttp::Unlink(const Path& path) {
  errno = ENOSYS;
  return -1;
}

int MountHttp::Mkdir(const Path& path, int permissions) {
  errno = ENOSYS;
  return -1;
}

int MountHttp::Rmdir(const Path& path) {
  errno = ENOSYS;
  return -1;
}

int MountHttp::Remove(const Path& path) {
  errno = ENOSYS;
  return -1;
}

PP_Resource MountHttp::MakeUrlRequestInfo(
    const std::string& url,
    const char* method,
    StringMap_t* additional_headers) {
  URLRequestInfoInterface* interface = ppapi_->GetURLRequestInfoInterface();
  VarInterface* var_interface = ppapi_->GetVarInterface();

  PP_Resource request_info = interface->Create(ppapi_->GetInstance());
  if (!request_info)
    return 0;

  interface->SetProperty(
      request_info, PP_URLREQUESTPROPERTY_URL,
      var_interface->VarFromUtf8(url.c_str(), url.length()));
  interface->SetProperty(request_info, PP_URLREQUESTPROPERTY_METHOD,
                         var_interface->VarFromUtf8(method, strlen(method)));
  interface->SetProperty(request_info,
                         PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS,
                         PP_MakeBool(allow_cors_ ? PP_TRUE : PP_FALSE));
  interface->SetProperty(request_info, PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS,
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
      iter != additional_headers->end(); ++iter) {
    headers += iter->first + ": " + iter->second + '\n';
  }

  interface->SetProperty(
      request_info, PP_URLREQUESTPROPERTY_HEADERS,
      var_interface->VarFromUtf8(headers.c_str(), headers.length()));

  return request_info;
}

MountHttp::MountHttp()
    : allow_cors_(false),
      allow_credentials_(false),
      cache_stat_(true),
      cache_content_(true) {
}

bool MountHttp::Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
  if (!Mount::Init(dev, args, ppapi))
    return false;

  // Parse mount args.
  for (StringMap_t::iterator iter = args.begin(); iter != args.end(); ++iter) {
    if (iter->first == "SOURCE") {
      url_root_ = iter->second;

      // Make sure url_root_ ends with a slash.
      if (!url_root_.empty() && url_root_[url_root_.length() - 1] != '/') {
        url_root_ += '/';
      }
    } else if (iter->first == "manifest") {
      char *text = LoadManifest(iter->second);
      if (text != NULL) {
        ParseManifest(text);
        delete[] text;
      }
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

  return true;
}

void MountHttp::Destroy() {
}

MountNodeDir* MountHttp::FindOrCreateDir(const Path& path) {
  std::string strpath = path.Join();
  NodeMap_t::iterator iter = node_cache_.find(strpath);
  if (iter != node_cache_.end()) {
    return static_cast<MountNodeDir*>(iter->second);
  }

  // If the node does not exist, create it, and add it to the node cache
  MountNodeDir* node = new MountNodeDir(this);
  node->Init(S_IREAD);
  node_cache_[strpath] = node;

  // If not the root node, find the parent node and add it to the parent
  if (!path.Top()) {
    MountNodeDir* parent = FindOrCreateDir(path.Parent());
    parent->AddChild(path.Basename(), node);
  }

  return node;
}

bool MountHttp::ParseManifest(char *text) {
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
        case '-': mode = S_IFREG; break;
        case 'c': mode = S_IFCHR; break;
        default:
          fprintf(stderr, "Unable to parse type %s for %s.\n", modestr, name);
          return false;
      }

      switch (modestr[1]) {
        case '-': break;
        case 'r': mode |= S_IREAD; break;
        default:
          fprintf(stderr, "Unable to parse read %s for %s.\n", modestr, name);
          return false;
      }

      switch (modestr[2]) {
        case '-': break;
        case 'w': mode |= S_IWRITE; break;
        default:
          fprintf(stderr, "Unable to parse write %s for %s.\n", modestr, name);
          return false;
      }

      Path path(name);
      std::string url = url_root_ + (path.IsAbsolute() ?
          path.Range(1, path.Size()) :
          path.Join());

      MountNodeHttp* node = new MountNodeHttp(this, url, cache_content_);
      node->Init(mode);
      node->SetCachedSize(atoi(lenstr));

      MountNodeDir* dir_node = FindOrCreateDir(path.Parent());
      dir_node->AddChild(path.Basename(), node);

      std::string pname = path.Join();
      node_cache_[pname] = node;
    }
  }

  return true;
}

char *MountHttp::LoadManifest(const std::string& manifest_name) {
  Path manifest_path(manifest_name);
  MountNode* manifest_node = Open(manifest_path, O_RDONLY);

  if (manifest_node) {
    char *text = new char[manifest_node->GetSize() + 1];
    off_t len = manifest_node->Read(0, text, manifest_node->GetSize());
    manifest_node->Release();

    text[len] = 0;
    return text;
  }

  fprintf(stderr, "Could not open manifest: %s\n", manifest_name.c_str());
  return NULL;
}
