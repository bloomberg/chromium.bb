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
  if (iter != error_map_.end())
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
                                     PP_Resource request_info,
                                     PP_CompletionCallback callback) {
  FakeURLLoaderResource* loader_resource =
      core_interface_->resource_manager()->Get<FakeURLLoaderResource>(loader);
  if (loader_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeURLRequestInfoResource* request_info_resource =
      core_interface_->resource_manager()->Get<FakeURLRequestInfoResource>(
          request_info);
  if (request_info_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  // Create a response resource.
  FakeURLResponseInfoResource* response = new FakeURLResponseInfoResource;
  loader_resource->response =
      CREATE_RESOURCE(core_interface_->resource_manager(),
                      FakeURLResponseInfoResource,
                      response);

  loader_resource->entity = NULL;
  loader_resource->read_offset = 0;
  loader_resource->read_end = 0;

  // Get the URL from the request info.
  std::string url = request_info_resource->url;
  std::string method = request_info_resource->method;

  response->url = url;
  // TODO(binji): allow this to be set?
  response->headers.clear();

  // Check the error map first, to see if this URL should produce an error.
  EXPECT_TRUE(NULL != loader_resource->server);
  int http_status_code = loader_resource->server->GetError(url);
  if (http_status_code != 0) {
    // Got an error, return that in the response.
    response->status_code = http_status_code;
  } else {
    // Look up the URL in the loader resource entity map.
    FakeURLLoaderEntity* entity = loader_resource->server->GetEntity(url);
    response->status_code = entity ? 200 : 404;

    if (method == "GET") {
      loader_resource->entity = entity;
    } else if (method == "HEAD") {
      // Do nothing, we only set the status code.
    } else {
      response->status_code = 405;  // Method not allowed.
    }

    if (entity != NULL) {
      size_t content_length = entity->body().size();
      loader_resource->read_end = content_length;

      if (loader_resource->server->send_content_length()) {
        std::ostringstream ss;
        ss << "Content-Length: " << content_length << "\n";
        response->headers += ss.str();
      }

      if (loader_resource->server->allow_partial()) {
        std::string headers = request_info_resource->headers;
        std::string range;
        if (GetHeaderValue(headers, "Range", &range)) {
          // We don't support all range requests, just bytes=<num>-<num>
          int lo;
          int hi;
          if (sscanf(range.c_str(), "bytes=%d-%d", &lo, &hi) == 2) {
            // The range is a closed interval; e.g. 0-10 is 11 bytes. We'll
            // store it as a half-open interval instead--it's more natural in
            // C that way.
            loader_resource->read_offset = lo;
            loader_resource->read_end = hi + 1;

            // Also add a "Content-Range" response header.
            std::ostringstream ss;
            ss << "Content-Range: "
               << lo << "-" << hi << "/" << content_length << "\n";
            response->headers += ss.str();

            response->status_code = 206;  // Partial content
          } else {
            // Couldn't parse the range.
            response->status_code = 416;  // Request range not satisfiable.
          }
        }
      }
    }
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
