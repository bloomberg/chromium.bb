// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_pepper_interface_url_loader.h"

#include <string.h>
#include <strings.h>

#include <algorithm>
#include <sstream>

#include "gtest/gtest.h"

namespace {

bool GetHeaderValue(const std::string& headers, const std::string& key,
                    std::string* out_value) {
  out_value->clear();

  size_t offset = 0;
  while (offset != std::string::npos) {
    // Find the next colon; this separates the key from the value.
    size_t colon = headers.find(':', offset);
    if (colon == std::string::npos)
      return false;

    // Find the newline; this separates the value from the next header.
    size_t newline = headers.find('\n', offset);
    if (strncasecmp(key.c_str(), &headers.data()[offset], key.size()) != 0) {
      // Key doesn't match, skip to next header.
      offset = newline;
      continue;
    }

    // Key matches, extract value. First, skip leading spaces.
    size_t nonspace = headers.find_first_not_of(' ', colon + 1);
    if (nonspace == std::string::npos)
      return false;

    out_value->assign(headers, nonspace, newline - nonspace);
    return true;
  }

  return false;
}

class FakeInstanceResource : public FakeResource {
 public:
  FakeInstanceResource() : server_template(NULL) {}
  static const char* classname() { return "FakeInstanceResource"; }

  FakeURLLoaderServer* server_template;  // Weak reference.
};

class FakeURLLoaderResource : public FakeResource {
 public:
  FakeURLLoaderResource()
      : manager(NULL),
        server(NULL),
        entity(NULL),
        response(0),
        read_offset(0) {}

  virtual void Destroy() {
    EXPECT_TRUE(manager != NULL);
    if (response != 0)
      manager->Release(response);
  }

  static const char* classname() { return "FakeURLLoaderResource"; }

  FakeResourceManager* manager;  // Weak reference.
  FakeURLLoaderServer* server;  // Weak reference.
  FakeURLLoaderEntity* entity;  // Weak reference.
  PP_Resource response;
  size_t read_offset;
  size_t read_end;
};

class FakeURLRequestInfoResource : public FakeResource {
 public:
  FakeURLRequestInfoResource() {}
  static const char* classname() { return "FakeURLRequestInfoResource"; }

  std::string url;
  std::string method;
  std::string headers;
};

class FakeURLResponseInfoResource : public FakeResource {
 public:
  FakeURLResponseInfoResource() : status_code(0) {}
  static const char* classname() { return "FakeURLResponseInfoResource"; }

  int status_code;
  std::string url;
  std::string headers;
};

// Helper function to call the completion callback if it is defined (an
// asynchronous call), or return the result directly if it isn't (a synchronous
// call).
//
// Use like this:
//   if (<some error condition>)
//     return RunCompletionCallback(callback, PP_ERROR_FUBAR);
//
//   /* Everything worked OK */
//   return RunCompletionCallback(callback, PP_OK);
int32_t RunCompletionCallback(PP_CompletionCallback* callback, int32_t result) {
  if (callback->func) {
    PP_RunCompletionCallback(callback, result);
    return PP_OK_COMPLETIONPENDING;
  }
  return result;
}

void HandleContentLength(FakeURLLoaderResource* loader,
                         FakeURLResponseInfoResource* response,
                         FakeURLLoaderEntity* entity) {
  size_t content_length = entity->body().size();
  if (!loader->server->send_content_length())
    return;

  std::ostringstream ss;
  ss << "Content-Length: " << content_length << "\n";
  response->headers += ss.str();
}

void HandlePartial(FakeURLLoaderResource* loader,
                   FakeURLRequestInfoResource* request,
                   FakeURLResponseInfoResource* response,
                   FakeURLLoaderEntity* entity) {
  if (!loader->server->allow_partial())
    return;

  // Read the RFC on byte ranges for more info:
  // http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.35.1
  std::string range;
  if (!GetHeaderValue(request->headers, "Range", &range))
    return;

  // We don't support all range requests, just bytes=<num>-<num>
  unsigned lo;
  unsigned hi;
  if (sscanf(range.c_str(), "bytes=%u-%u", &lo, &hi) != 2) {
    // Couldn't parse the range value.
    return;
  }

  size_t content_length = entity->body().size();
  if (lo > content_length) {
    // Trying to start reading past the end of the entity is
    // unsatisfiable.
    response->status_code = 416;  // Request range not satisfiable.
    return;
  }

  // Clamp the hi value to the content length.
  if (hi >= content_length)
    hi = content_length - 1;

  if (lo > hi) {
    // Bad range, ignore it and return the full result.
    return;
  }

  // The range is a closed interval; e.g. 0-10 is 11 bytes. We'll
  // store it as a half-open interval instead--it's more natural
  // in C that way.
  loader->read_offset = lo;
  loader->read_end = hi + 1;

  // Also add a "Content-Range" response header.
  std::ostringstream ss;
  ss << "Content-Range: " << lo << "-" << hi << "/" << content_length << "\n";
  response->headers += ss.str();

  response->status_code = 206;  // Partial content
}

}  // namespace

FakeURLLoaderEntity::FakeURLLoaderEntity(const std::string& body)
    : body_(body) {}

FakeURLLoaderServer::FakeURLLoaderServer()
    : max_read_size_(0), send_content_length_(false), allow_partial_(false) {}

void FakeURLLoaderServer::Clear() {
  entity_map_.clear();
}

bool FakeURLLoaderServer::AddEntity(const std::string& url,
                                    const std::string& body,
                                    FakeURLLoaderEntity** out_entity) {
  EntityMap::iterator iter = entity_map_.find(url);
  if (iter != entity_map_.end()) {
    if (out_entity)
      *out_entity = NULL;
    return false;
  }

  FakeURLLoaderEntity entity(body);
  std::pair<EntityMap::iterator, bool> result =
      entity_map_.insert(EntityMap::value_type(url, entity));

  EXPECT_EQ(true, result.second);
  if (out_entity)
    *out_entity = &result.first->second;
  return true;
}

bool FakeURLLoaderServer::AddError(const std::string& url,
                                   int http_status_code) {
  ErrorMap::iterator iter = error_map_.find(url);
  if (iter != error_map_.end())
    return false;

  error_map_[url] = http_status_code;
  return true;
}

FakeURLLoaderEntity* FakeURLLoaderServer::GetEntity(const std::string& url) {
  EntityMap::iterator iter = entity_map_.find(url);
  if (iter == entity_map_.end())
    return NULL;
  return &iter->second;
}

int FakeURLLoaderServer::GetError(const std::string& url) {
  ErrorMap::iterator iter = error_map_.find(url);
  if (iter == error_map_.end())
    return 0;
  return iter->second;
}

FakeURLLoaderInterface::FakeURLLoaderInterface(
    FakeCoreInterface* core_interface)
    : core_interface_(core_interface) {}

PP_Resource FakeURLLoaderInterface::Create(PP_Instance instance) {
  FakeInstanceResource* instance_resource =
      core_interface_->resource_manager()->Get<FakeInstanceResource>(instance);
  if (instance_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeURLLoaderResource* loader_resource = new FakeURLLoaderResource;
  loader_resource->manager = core_interface_->resource_manager();
  loader_resource->server =
      new FakeURLLoaderServer(*instance_resource->server_template);

  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeURLLoaderResource,
                         loader_resource);
}

int32_t FakeURLLoaderInterface::Open(PP_Resource loader,
                                     PP_Resource request,
                                     PP_CompletionCallback callback) {
  FakeURLLoaderResource* loader_resource =
      core_interface_->resource_manager()->Get<FakeURLLoaderResource>(loader);
  if (loader_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeURLRequestInfoResource* request_resource =
      core_interface_->resource_manager()->Get<FakeURLRequestInfoResource>(
          request);
  if (request_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  // Create a response resource.
  FakeURLResponseInfoResource* response_resource =
      new FakeURLResponseInfoResource;
  loader_resource->response =
      CREATE_RESOURCE(core_interface_->resource_manager(),
                      FakeURLResponseInfoResource,
                      response_resource);

  loader_resource->entity = NULL;
  loader_resource->read_offset = 0;
  loader_resource->read_end = 0;

  // Get the URL from the request info.
  std::string url = request_resource->url;
  std::string method = request_resource->method;

  response_resource->url = url;
  // TODO(binji): allow this to be set?
  response_resource->headers.clear();

  // Check the error map first, to see if this URL should produce an error.
  EXPECT_TRUE(NULL != loader_resource->server);
  int http_status_code = loader_resource->server->GetError(url);
  if (http_status_code != 0) {
    // Got an error, return that in the response.
    response_resource->status_code = http_status_code;
    return RunCompletionCallback(&callback, PP_OK);
  }

  // Look up the URL in the loader resource entity map.
  FakeURLLoaderEntity* entity = loader_resource->server->GetEntity(url);
  response_resource->status_code = entity ? 200 : 404;

  if (method == "GET") {
    loader_resource->entity = entity;
  } else if (method != "HEAD") {
    response_resource->status_code = 405;  // Method not allowed.
    return RunCompletionCallback(&callback, PP_OK);
  }

  if (entity != NULL) {
    size_t content_length = entity->body().size();
    loader_resource->read_end = content_length;
    HandleContentLength(loader_resource, response_resource, entity);
    HandlePartial(loader_resource, request_resource, response_resource, entity);
  }

  // Call the callback.
  return RunCompletionCallback(&callback, PP_OK);
}

PP_Resource FakeURLLoaderInterface::GetResponseInfo(PP_Resource loader) {
  FakeURLLoaderResource* loader_resource =
      core_interface_->resource_manager()->Get<FakeURLLoaderResource>(loader);
  if (loader_resource == NULL)
    return 0;

  // Returned resources have an implicit AddRef.
  core_interface_->resource_manager()->AddRef(loader_resource->response);
  return loader_resource->response;
}

int32_t FakeURLLoaderInterface::ReadResponseBody(
    PP_Resource loader,
    void* buffer,
    int32_t bytes_to_read,
    PP_CompletionCallback callback) {
  FakeURLLoaderResource* loader_resource =
      core_interface_->resource_manager()->Get<FakeURLLoaderResource>(loader);
  if (loader_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (loader_resource->entity == NULL)
    // TODO(binji): figure out the correct error here.
    return PP_ERROR_FAILED;

  const std::string& body = loader_resource->entity->body();
  size_t offset = loader_resource->read_offset;
  // Never read more than is available.
  size_t max_readable = std::max<size_t>(0, body.length() - offset);
  size_t server_max_read_size = loader_resource->server->max_read_size();
  // Allow the test to specify how much the "server" should send in each call
  // to ReadResponseBody. A max_read_size of 0 means read as much as the
  // buffer will allow.
  if (server_max_read_size != 0)
    max_readable = std::min(max_readable, server_max_read_size);

  bytes_to_read = std::min(static_cast<size_t>(bytes_to_read), max_readable);
  memcpy(buffer, &body.data()[offset], bytes_to_read);
  loader_resource->read_offset += bytes_to_read;

  return RunCompletionCallback(&callback, bytes_to_read);
}

void FakeURLLoaderInterface::Close(PP_Resource loader) {
  FakeURLLoaderResource* loader_resource =
      core_interface_->resource_manager()->Get<FakeURLLoaderResource>(loader);
  if (loader_resource == NULL)
    return;

  core_interface_->resource_manager()->Release(loader_resource->response);

  loader_resource->server = NULL;
  loader_resource->entity = NULL;
  loader_resource->response = 0;
  loader_resource->read_offset = 0;
}

FakeURLRequestInfoInterface::FakeURLRequestInfoInterface(
    FakeCoreInterface* core_interface,
    FakeVarInterface* var_interface)
    : core_interface_(core_interface), var_interface_(var_interface) {}

PP_Resource FakeURLRequestInfoInterface::Create(PP_Instance instance) {
  FakeInstanceResource* instance_resource =
      core_interface_->resource_manager()->Get<FakeInstanceResource>(instance);
  if (instance_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeURLRequestInfoResource,
                         new FakeURLRequestInfoResource);
}

PP_Bool FakeURLRequestInfoInterface::SetProperty(PP_Resource request,
                                                 PP_URLRequestProperty property,
                                                 PP_Var value) {
  FakeURLRequestInfoResource* request_resource =
      core_interface_->resource_manager()->Get<FakeURLRequestInfoResource>(
          request);
  if (request_resource == NULL)
    return PP_FALSE;

  switch (property) {
    case PP_URLREQUESTPROPERTY_URL: {
      if (value.type != PP_VARTYPE_STRING)
        return PP_FALSE;

      uint32_t len;
      const char* url = var_interface_->VarToUtf8(value, &len);
      if (url == NULL)
        return PP_FALSE;

      request_resource->url = url;
      var_interface_->Release(value);
      return PP_TRUE;
    }
    case PP_URLREQUESTPROPERTY_METHOD: {
      if (value.type != PP_VARTYPE_STRING)
        return PP_FALSE;

      uint32_t len;
      const char* url = var_interface_->VarToUtf8(value, &len);
      if (url == NULL)
        return PP_FALSE;

      request_resource->method = url;
      var_interface_->Release(value);
      return PP_TRUE;
    }
    case PP_URLREQUESTPROPERTY_HEADERS: {
      if (value.type != PP_VARTYPE_STRING)
        return PP_FALSE;

      uint32_t len;
      const char* url = var_interface_->VarToUtf8(value, &len);
      if (url == NULL)
        return PP_FALSE;

      request_resource->headers = url;
      var_interface_->Release(value);
      return PP_TRUE;
    }
    case PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS: {
      if (value.type != PP_VARTYPE_BOOL)
        return PP_FALSE;
      // Throw the value away for now. TODO(binji): add tests for this.
      return PP_TRUE;
    }
    case PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS: {
      if (value.type != PP_VARTYPE_BOOL)
        return PP_FALSE;
      // Throw the value away for now. TODO(binji): add tests for this.
      return PP_TRUE;
    }
    default:
      EXPECT_TRUE(false) << "Unimplemented property " << property
                         << " in "
                            "FakeURLRequestInfoInterface::SetProperty";
      return PP_FALSE;
  }
}

FakeURLResponseInfoInterface::FakeURLResponseInfoInterface(
    FakeCoreInterface* core_interface,
    FakeVarInterface* var_interface)
    : core_interface_(core_interface), var_interface_(var_interface) {}

PP_Var FakeURLResponseInfoInterface::GetProperty(
    PP_Resource response,
    PP_URLResponseProperty property) {
  FakeURLResponseInfoResource* response_resource =
      core_interface_->resource_manager()->Get<FakeURLResponseInfoResource>(
          response);
  if (response_resource == NULL)
    return PP_Var();

  switch (property) {
    case PP_URLRESPONSEPROPERTY_URL:
      return var_interface_->VarFromUtf8(response_resource->url.data(),
                                         response_resource->url.size());

    case PP_URLRESPONSEPROPERTY_STATUSCODE:
      return PP_MakeInt32(response_resource->status_code);

    case PP_URLRESPONSEPROPERTY_HEADERS:
      return var_interface_->VarFromUtf8(response_resource->headers.data(),
                                         response_resource->headers.size());
    default:
      EXPECT_TRUE(false) << "Unimplemented property " << property
                         << " in "
                            "FakeURLResponseInfoInterface::GetProperty";
      return PP_Var();
  }
}

FakePepperInterfaceURLLoader::FakePepperInterfaceURLLoader()
    : url_loader_interface_(&core_interface_),
      url_request_info_interface_(&core_interface_, &var_interface_),
      url_response_info_interface_(&core_interface_, &var_interface_) {
  FakeInstanceResource* instance_resource = new FakeInstanceResource;
  instance_resource->server_template = &server_template_;
  instance_ = CREATE_RESOURCE(core_interface_.resource_manager(),
                              FakeInstanceResource,
                              instance_resource);
}

FakePepperInterfaceURLLoader::~FakePepperInterfaceURLLoader() {
  core_interface_.ReleaseResource(instance_);
}

nacl_io::CoreInterface* FakePepperInterfaceURLLoader::GetCoreInterface() {
  return &core_interface_;
}

nacl_io::VarInterface* FakePepperInterfaceURLLoader::GetVarInterface() {
  return &var_interface_;
}

nacl_io::URLLoaderInterface*
FakePepperInterfaceURLLoader::GetURLLoaderInterface() {
  return &url_loader_interface_;
}

nacl_io::URLRequestInfoInterface*
FakePepperInterfaceURLLoader::GetURLRequestInfoInterface() {
  return &url_request_info_interface_;
}

nacl_io::URLResponseInfoInterface*
FakePepperInterfaceURLLoader::GetURLResponseInfoInterface() {
  return &url_response_info_interface_;
}
