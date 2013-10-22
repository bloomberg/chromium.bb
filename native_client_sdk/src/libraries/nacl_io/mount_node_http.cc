// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_node_http.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <ppapi/c/pp_errors.h>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/mount_http.h"
#include "nacl_io/osinttypes.h"

#if defined(WIN32)
#define snprintf _snprintf
#endif

namespace nacl_io {

namespace {

// If we're attempting to read a partial request, but the server returns a full
// request, we need to read all of the data up to the start of our partial
// request into a dummy buffer. This is the maximum size of that buffer.
const size_t MAX_READ_BUFFER_SIZE = 64 * 1024;
const int32_t STATUSCODE_OK = 200;
const int32_t STATUSCODE_PARTIAL_CONTENT = 206;
const int32_t STATUSCODE_FORBIDDEN = 403;
const int32_t STATUSCODE_NOT_FOUND = 404;

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

bool ParseContentRange(const StringMap_t& headers,
                       size_t* read_start,
                       size_t* read_end,
                       size_t* entity_length) {
  StringMap_t::const_iterator iter = headers.find("Content-Range");
  if (iter == headers.end())
    return false;

  // The key should look like "bytes ##-##/##" or "bytes ##-##/*". The last
  // value is the entity length, which can potentially be * (i.e. unknown).
  int read_start_int;
  int read_end_int;
  int entity_length_int;
  int result = sscanf(iter->second.c_str(),
                      "bytes %" SCNuS "-%" SCNuS "/%" SCNuS,
                      &read_start_int,
                      &read_end_int,
                      &entity_length_int);

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

// Maps an HTTP |status_code| onto the appropriate errno code.
int HTTPStatusCodeToErrno(int status_code) {
  switch (status_code) {
    case STATUSCODE_OK:
    case STATUSCODE_PARTIAL_CONTENT:
      return 0;
    case STATUSCODE_FORBIDDEN:
      return EACCES;
    case STATUSCODE_NOT_FOUND:
      return ENOENT;
  }
  if (status_code >= 400 && status_code < 500)
    return EINVAL;
  return EIO;
}

}  // namespace

void MountNodeHttp::SetCachedSize(off_t size) {
  has_cached_size_ = true;
  stat_.st_size = size;
}

Error MountNodeHttp::FSync() { return ENOSYS; }

Error MountNodeHttp::GetDents(size_t offs,
                              struct dirent* pdir,
                              size_t count,
                              int* out_bytes) {
  *out_bytes = 0;
  return ENOSYS;
}

Error MountNodeHttp::GetStat(struct stat* stat) {
  AUTO_LOCK(node_lock_);

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
    Error error = OpenUrl("HEAD",
                          &headers,
                          &loader,
                          &request,
                          &response,
                          &statuscode,
                          &response_headers);
    if (error)
      return error;

    ScopedResource scoped_loader(mount_->ppapi(), loader);
    ScopedResource scoped_request(mount_->ppapi(), request);
    ScopedResource scoped_response(mount_->ppapi(), response);

    size_t entity_length;
    if (ParseContentLength(response_headers, &entity_length)) {
      SetCachedSize(static_cast<off_t>(entity_length));
    } else if (cache_content_ && !has_cached_size_) {
      error = DownloadToCache();
      // TODO(binji): this error should not be dropped, but it requires a bit
      // of a refactor of the tests. See crbug.com/245431
      // if (error)
      //   return error;
    } else {
      // Don't use SetCachedSize here -- it is actually unknown.
      stat_.st_size = 0;
    }

    stat_.st_atime = 0;  // TODO(binji): Use "Last-Modified".
    stat_.st_mtime = 0;
    stat_.st_ctime = 0;

    stat_.st_mode |= S_IFREG;
  }

  // Fill the stat structure if provided
  if (stat)
    *stat = stat_;

  return 0;
}

Error MountNodeHttp::Read(const HandleAttr& attr,
                          void* buf,
                          size_t count,
                          int* out_bytes) {
  *out_bytes = 0;

  AUTO_LOCK(node_lock_);
  if (cache_content_) {
    if (cached_data_.empty()) {
      Error error = DownloadToCache();
      if (error)
        return error;
    }

    return ReadPartialFromCache(attr.offs, buf, count, out_bytes);
  }

  return DownloadPartial(attr.offs, buf, count, out_bytes);
}

Error MountNodeHttp::FTruncate(off_t size) { return ENOSYS; }

Error MountNodeHttp::Write(const HandleAttr& attr,
                           const void* buf,
                           size_t count,
                           int* out_bytes) {
  // TODO(binji): support POST?
  *out_bytes = 0;
  return ENOSYS;
}

Error MountNodeHttp::GetSize(size_t* out_size) {
  *out_size = 0;

  // TODO(binji): This value should be cached properly; i.e. obey the caching
  // headers returned by the server.
  AUTO_LOCK(node_lock_);
  if (!has_cached_size_) {
    // Even if DownloadToCache fails, the best result we can return is what
    // was written to stat_.st_size.
    if (cache_content_) {
      Error error = DownloadToCache();
      if (error)
        return error;
    }
  }

  *out_size = stat_.st_size;
  return 0;
}

MountNodeHttp::MountNodeHttp(Mount* mount,
                             const std::string& url,
                             bool cache_content)
    : MountNode(mount),
      url_(url),
      cache_content_(cache_content),
      has_cached_size_(false) {}

void MountNodeHttp::SetMode(int mode) {
  stat_.st_mode = mode;
}

Error MountNodeHttp::OpenUrl(const char* method,
                             StringMap_t* request_headers,
                             PP_Resource* out_loader,
                             PP_Resource* out_request,
                             PP_Resource* out_response,
                             int32_t* out_statuscode,
                             StringMap_t* out_response_headers) {
  // Assume lock_ is already held.
  PepperInterface* ppapi = mount_->ppapi();

  MountHttp* mount_http = static_cast<MountHttp*>(mount_);
  ScopedResource request(
      ppapi, mount_http->MakeUrlRequestInfo(url_, method, request_headers));
  if (!request.pp_resource())
    return EINVAL;

  URLLoaderInterface* loader_interface = ppapi->GetURLLoaderInterface();
  URLResponseInfoInterface* response_interface =
      ppapi->GetURLResponseInfoInterface();
  VarInterface* var_interface = ppapi->GetVarInterface();

  ScopedResource loader(ppapi, loader_interface->Create(ppapi->GetInstance()));
  if (!loader.pp_resource())
    return EINVAL;

  int32_t result = loader_interface->Open(
      loader.pp_resource(), request.pp_resource(), PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPErrorToErrno(result);

  ScopedResource response(
      ppapi, loader_interface->GetResponseInfo(loader.pp_resource()));
  if (!response.pp_resource())
    return EINVAL;

  // Get response statuscode.
  PP_Var statuscode = response_interface->GetProperty(
      response.pp_resource(), PP_URLRESPONSEPROPERTY_STATUSCODE);

  if (statuscode.type != PP_VARTYPE_INT32)
    return EINVAL;

  *out_statuscode = statuscode.value.as_int;

  // Only accept OK or Partial Content.
  Error error = HTTPStatusCodeToErrno(*out_statuscode);
  if (error)
    return error;

  // Get response headers.
  PP_Var response_headers_var = response_interface->GetProperty(
      response.pp_resource(), PP_URLRESPONSEPROPERTY_HEADERS);

  uint32_t response_headers_length;
  const char* response_headers_str =
      var_interface->VarToUtf8(response_headers_var, &response_headers_length);

  *out_loader = loader.Release();
  *out_request = request.Release();
  *out_response = response.Release();
  *out_response_headers =
      ParseHeaders(response_headers_str, response_headers_length);

  return 0;
}

Error MountNodeHttp::DownloadToCache() {
  StringMap_t headers;
  PP_Resource loader;
  PP_Resource request;
  PP_Resource response;
  int32_t statuscode;
  StringMap_t response_headers;
  Error error = OpenUrl("GET",
                        &headers,
                        &loader,
                        &request,
                        &response,
                        &statuscode,
                        &response_headers);
  if (error)
    return error;

  PepperInterface* ppapi = mount_->ppapi();
  ScopedResource scoped_loader(ppapi, loader);
  ScopedResource scoped_request(ppapi, request);
  ScopedResource scoped_response(ppapi, response);

  size_t content_length = 0;
  if (ParseContentLength(response_headers, &content_length)) {
    cached_data_.resize(content_length);
    int real_size;
    error = DownloadToBuffer(
        loader, cached_data_.data(), content_length, &real_size);
    if (error)
      return error;

    SetCachedSize(real_size);
    cached_data_.resize(real_size);
    return 0;
  }

  // We don't know how big the file is. Read in chunks.
  cached_data_.resize(MAX_READ_BUFFER_SIZE);
  int total_bytes_read = 0;
  int bytes_to_read = MAX_READ_BUFFER_SIZE;
  while (true) {
    char* buf = cached_data_.data() + total_bytes_read;
    int bytes_read;
    error = DownloadToBuffer(loader, buf, bytes_to_read, &bytes_read);
    if (error)
      return error;

    total_bytes_read += bytes_read;

    if (bytes_read < bytes_to_read) {
      SetCachedSize(total_bytes_read);
      cached_data_.resize(total_bytes_read);
      return 0;
    }

    cached_data_.resize(total_bytes_read + bytes_to_read);
  }
}

Error MountNodeHttp::ReadPartialFromCache(size_t offs,
                                          void* buf,
                                          int count,
                                          int* out_bytes) {
  *out_bytes = 0;

  if (offs > cached_data_.size())
    return EINVAL;

  count = std::min(count, static_cast<int>(cached_data_.size() - offs));
  memcpy(buf, &cached_data_.data()[offs], count);

  *out_bytes = count;
  return 0;
}

Error MountNodeHttp::DownloadPartial(size_t offs,
                                     void* buf,
                                     size_t count,
                                     int* out_bytes) {
  *out_bytes = 0;

  StringMap_t headers;

  char buffer[100];
  // Range request is inclusive: 0-99 returns 100 bytes.
  snprintf(&buffer[0],
           sizeof(buffer),
           "bytes=%" PRIuS "-%" PRIuS,
           offs,
           offs + count - 1);
  headers["Range"] = buffer;

  PP_Resource loader;
  PP_Resource request;
  PP_Resource response;
  int32_t statuscode;
  StringMap_t response_headers;
  Error error = OpenUrl("GET",
                        &headers,
                        &loader,
                        &request,
                        &response,
                        &statuscode,
                        &response_headers);
  if (error)
    return error;

  PepperInterface* ppapi = mount_->ppapi();
  ScopedResource scoped_loader(ppapi, loader);
  ScopedResource scoped_request(ppapi, request);
  ScopedResource scoped_response(ppapi, response);

  size_t read_start = 0;
  if (statuscode == STATUSCODE_OK) {
    // No partial result, read everything starting from the part we care about.
    size_t content_length;
    if (ParseContentLength(response_headers, &content_length)) {
      if (offs >= content_length)
        return EINVAL;

      // Clamp count, if trying to read past the end of the file.
      if (offs + count > content_length) {
        count = content_length - offs;
      }
    }
  } else if (statuscode == STATUSCODE_PARTIAL_CONTENT) {
    // Determine from the headers where we are reading.
    size_t read_end;
    size_t entity_length;
    if (ParseContentRange(
            response_headers, &read_start, &read_end, &entity_length)) {
      if (read_start > offs || read_start > read_end) {
        // If this error occurs, the server is returning bogus values.
        return EINVAL;
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
      int32_t bytes_read;
      Error error =
          DownloadToBuffer(loader, buffer_.data(), buffer_.size(), &bytes_read);
      if (error)
        return error;

      bytes_to_read -= bytes_read;
    }
  }

  return DownloadToBuffer(loader, buf, count, out_bytes);
}

Error MountNodeHttp::DownloadToBuffer(PP_Resource loader,
                                      void* buf,
                                      int count,
                                      int* out_bytes) {
  *out_bytes = 0;

  PepperInterface* ppapi = mount_->ppapi();
  URLLoaderInterface* loader_interface = ppapi->GetURLLoaderInterface();

  char* out_buffer = static_cast<char*>(buf);
  int bytes_to_read = count;
  while (bytes_to_read > 0) {
    int bytes_read = loader_interface->ReadResponseBody(
        loader, out_buffer, bytes_to_read, PP_BlockUntilComplete());

    if (bytes_read == 0) {
      // This is not an error -- it may just be that we were trying to read
      // more data than exists.
      *out_bytes = count - bytes_to_read;
      return 0;
    }

    if (bytes_read < 0)
      return PPErrorToErrno(bytes_read);

    assert(bytes_read <= bytes_to_read);
    bytes_to_read -= bytes_read;
    out_buffer += bytes_read;
  }

  *out_bytes = count;
  return 0;
}

}  // namespace nacl_io

