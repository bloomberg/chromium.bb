// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Web request notifier implementation.
#include "ceee/ie/plugin/bho/webrequest_notifier.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome_frame/function_stub.h"
#include "chrome_frame/utils.h"

namespace {

const wchar_t kUrlMonModuleName[] = L"urlmon.dll";
const char kWinINetModuleName[] = "wininet.dll";
const char kInternetSetStatusCallbackAFunctionName[] =
    "InternetSetStatusCallbackA";
const char kInternetSetStatusCallbackWFunctionName[] =
    "InternetSetStatusCallbackW";
const char kInternetConnectAFunctionName[] = "InternetConnectA";
const char kInternetConnectWFunctionName[] = "InternetConnectW";
const char kHttpOpenRequestAFunctionName[] = "HttpOpenRequestA";
const char kHttpOpenRequestWFunctionName[] = "HttpOpenRequestW";
const char kHttpSendRequestAFunctionName[] = "HttpSendRequestA";
const char kHttpSendRequestWFunctionName[] = "HttpSendRequestW";
const char kInternetReadFileFunctionName[] = "InternetReadFile";

}  // namespace

WebRequestNotifier::WebRequestNotifier()
        : internet_status_callback_stub_(NULL),
          start_count_(0),
          initialize_state_(NOT_INITIALIZED),
          webrequest_events_funnel_(new BrokerRpcClient) {
}

WebRequestNotifier::~WebRequestNotifier() {
  DCHECK_EQ(start_count_, 0);
}

bool WebRequestNotifier::RequestToStart() {
  {
    CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
    start_count_++;

    if (initialize_state_ != NOT_INITIALIZED)
      return initialize_state_ != FAILED_TO_INITIALIZE;
    initialize_state_ = INITIALIZING;
  }

  bool success = false;
  do {
    // We are not going to unpatch any of the patched WinINet functions or the
    // status callback function. Instead, we pin our DLL in memory so that all
    // the patched functions can be accessed until the process goes away.
    PinModule();

    PatchWinINetFunction(kInternetSetStatusCallbackAFunctionName,
                         &internet_set_status_callback_a_patch_,
                         InternetSetStatusCallbackAPatch);
    PatchWinINetFunction(kInternetSetStatusCallbackWFunctionName,
                         &internet_set_status_callback_w_patch_,
                         InternetSetStatusCallbackWPatch);
    if (!HasPatchedOneVersion(internet_set_status_callback_a_patch_,
                              internet_set_status_callback_w_patch_)) {
      break;
    }

    PatchWinINetFunction(kInternetConnectAFunctionName,
                         &internet_connect_a_patch_,
                         InternetConnectAPatch);
    PatchWinINetFunction(kInternetConnectWFunctionName,
                         &internet_connect_w_patch_,
                         InternetConnectWPatch);
    if (!HasPatchedOneVersion(internet_connect_a_patch_,
                              internet_connect_w_patch_)) {
      break;
    }

    PatchWinINetFunction(kHttpOpenRequestAFunctionName,
                         &http_open_request_a_patch_,
                         HttpOpenRequestAPatch);
    PatchWinINetFunction(kHttpOpenRequestWFunctionName,
                         &http_open_request_w_patch_,
                         HttpOpenRequestWPatch);
    if (!HasPatchedOneVersion(http_open_request_a_patch_,
                              http_open_request_w_patch_)) {
      break;
    }

    PatchWinINetFunction(kHttpSendRequestAFunctionName,
                         &http_send_request_a_patch_,
                         HttpSendRequestAPatch);
    PatchWinINetFunction(kHttpSendRequestWFunctionName,
                         &http_send_request_w_patch_,
                         HttpSendRequestWPatch);
    if (!HasPatchedOneVersion(http_send_request_a_patch_,
                              http_send_request_w_patch_)) {
      break;
    }

    PatchWinINetFunction(kInternetReadFileFunctionName,
                         &internet_read_file_patch_,
                         InternetReadFilePatch);
    if (!internet_read_file_patch_.is_patched())
      break;

    success = true;
  } while (false);

  {
    CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
    initialize_state_ = success ? SUCCEEDED_TO_INITIALIZE :
                                  FAILED_TO_INITIALIZE;
  }
  return success;
}

void WebRequestNotifier::RequestToStop() {
  CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
  if (start_count_ <= 0) {
    NOTREACHED();
    return;
  }

  start_count_--;
  if (start_count_ == 0) {
    // It is supposed that every handle must be closed using
    // InternetCloseHandle(). However, IE seems to leak handles in some (rare)
    // cases. For example, when the current page is a JPG or GIF image and the
    // user refreshes the page.
    // If that happens, the server_map_ and request_map_ won't be empty.
    LOG_IF(WARNING, !server_map_.empty() || !request_map_.empty())
        << "There are Internet handles that haven't been closed when "
        << "WebRequestNotifier stops.";

    for (RequestMap::iterator iter = request_map_.begin();
         iter != request_map_.end(); ++iter) {
      TransitRequestToNextState(RequestInfo::ERROR_OCCURRED, &iter->second);
    }

    server_map_.clear();
    request_map_.clear();
  }
}

void WebRequestNotifier::PatchWinINetFunction(
    const char* name,
    app::win::IATPatchFunction* patch_function,
    void* handler) {
  DWORD error = patch_function->Patch(kUrlMonModuleName, kWinINetModuleName,
                                      name, handler);
  // The patching operation is either successful, or failed cleanly.
  DCHECK(error == NO_ERROR || !patch_function->is_patched());
}

INTERNET_STATUS_CALLBACK STDAPICALLTYPE
    WebRequestNotifier::InternetSetStatusCallbackAPatch(
        HINTERNET internet,
        INTERNET_STATUS_CALLBACK callback) {
  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  INTERNET_STATUS_CALLBACK new_callback =
      instance->HandleBeforeInternetSetStatusCallback(internet, callback);
  return ::InternetSetStatusCallbackA(internet, new_callback);
}

INTERNET_STATUS_CALLBACK STDAPICALLTYPE
    WebRequestNotifier::InternetSetStatusCallbackWPatch(
        HINTERNET internet,
        INTERNET_STATUS_CALLBACK callback) {
  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  INTERNET_STATUS_CALLBACK new_callback =
      instance->HandleBeforeInternetSetStatusCallback(internet, callback);
  return ::InternetSetStatusCallbackW(internet, new_callback);
}

HINTERNET STDAPICALLTYPE WebRequestNotifier::InternetConnectAPatch(
    HINTERNET internet,
    LPCSTR server_name,
    INTERNET_PORT server_port,
    LPCSTR user_name,
    LPCSTR password,
    DWORD service,
    DWORD flags,
    DWORD_PTR context) {
  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  instance->HandleBeforeInternetConnect(internet);

  HINTERNET server = ::InternetConnectA(internet, server_name, server_port,
                                        user_name, password, service, flags,
                                        context);

  instance->HandleAfterInternetConnect(server, CA2W(server_name), server_port,
                                       service);
  return server;
}

HINTERNET STDAPICALLTYPE WebRequestNotifier::InternetConnectWPatch(
    HINTERNET internet,
    LPCWSTR server_name,
    INTERNET_PORT server_port,
    LPCWSTR user_name,
    LPCWSTR password,
    DWORD service,
    DWORD flags,
    DWORD_PTR context) {
  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  instance->HandleBeforeInternetConnect(internet);

  HINTERNET server = ::InternetConnectW(internet, server_name, server_port,
                                        user_name, password, service, flags,
                                        context);

  instance->HandleAfterInternetConnect(server, server_name, server_port,
                                       service);
  return server;
}

HINTERNET STDAPICALLTYPE WebRequestNotifier::HttpOpenRequestAPatch(
    HINTERNET connect,
    LPCSTR verb,
    LPCSTR object_name,
    LPCSTR version,
    LPCSTR referrer,
    LPCSTR* accept_types,
    DWORD flags,
    DWORD_PTR context) {
  HINTERNET request = ::HttpOpenRequestA(connect, verb, object_name, version,
                                         referrer, accept_types, flags,
                                         context);

  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  instance->HandleAfterHttpOpenRequest(connect, request, verb,
                                       CA2W(object_name), flags);
  return request;
}

HINTERNET STDAPICALLTYPE WebRequestNotifier::HttpOpenRequestWPatch(
    HINTERNET connect,
    LPCWSTR verb,
    LPCWSTR object_name,
    LPCWSTR version,
    LPCWSTR referrer,
    LPCWSTR* accept_types,
    DWORD flags,
    DWORD_PTR context) {
  HINTERNET request = ::HttpOpenRequestW(connect, verb, object_name, version,
                                         referrer, accept_types, flags,
                                         context);

  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  instance->HandleAfterHttpOpenRequest(connect, request, CW2A(verb),
                                       object_name, flags);
  return request;
}

BOOL STDAPICALLTYPE WebRequestNotifier::HttpSendRequestAPatch(
    HINTERNET request,
    LPCSTR headers,
    DWORD headers_length,
    LPVOID optional,
    DWORD optional_length) {
  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  instance->HandleBeforeHttpSendRequest(request);
  return ::HttpSendRequestA(request, headers, headers_length, optional,
                            optional_length);
}

BOOL STDAPICALLTYPE WebRequestNotifier::HttpSendRequestWPatch(
    HINTERNET request,
    LPCWSTR headers,
    DWORD headers_length,
    LPVOID optional,
    DWORD optional_length) {
  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  instance->HandleBeforeHttpSendRequest(request);
  return ::HttpSendRequestW(request, headers, headers_length, optional,
                            optional_length);
}

void CALLBACK WebRequestNotifier::InternetStatusCallbackPatch(
    INTERNET_STATUS_CALLBACK original,
    HINTERNET internet,
    DWORD_PTR context,
    DWORD internet_status,
    LPVOID status_information,
    DWORD status_information_length) {
  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  instance->HandleBeforeInternetStatusCallback(original, internet, context,
                                               internet_status,
                                               status_information,
                                               status_information_length);
  original(internet, context, internet_status, status_information,
           status_information_length);
}

BOOL STDAPICALLTYPE WebRequestNotifier::InternetReadFilePatch(
    HINTERNET file,
    LPVOID buffer,
    DWORD number_of_bytes_to_read,
    LPDWORD number_of_bytes_read) {
  BOOL result = ::InternetReadFile(file, buffer, number_of_bytes_to_read,
                                   number_of_bytes_read);
  WebRequestNotifier* instance = ProductionWebRequestNotifier::get();
  instance->HandleAfterInternetReadFile(file, result, number_of_bytes_read);

  return result;
}

INTERNET_STATUS_CALLBACK
    WebRequestNotifier::HandleBeforeInternetSetStatusCallback(
        HINTERNET internet,
        INTERNET_STATUS_CALLBACK internet_callback) {
  CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
  if (IsNotRunning())
    return internet_callback;

  if (internet_callback == NULL)
    return NULL;

  if (internet_status_callback_stub_ == NULL) {
    return CreateInternetStatusCallbackStub(internet_callback);
  } else {
    if (internet_status_callback_stub_->argument() !=
        reinterpret_cast<uintptr_t>(internet_callback)) {
      NOTREACHED();
      return CreateInternetStatusCallbackStub(internet_callback);
    } else {
      return reinterpret_cast<INTERNET_STATUS_CALLBACK>(
          internet_status_callback_stub_->code());
    }
  }
}

void WebRequestNotifier::HandleBeforeInternetConnect(HINTERNET internet) {
  CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
  if (IsNotRunning())
    return;

  if (internet_status_callback_stub_ == NULL) {
    INTERNET_STATUS_CALLBACK original_callback =
        ::InternetSetStatusCallbackA(internet, NULL);
    if (original_callback != NULL) {
      INTERNET_STATUS_CALLBACK new_callback = CreateInternetStatusCallbackStub(
          original_callback);
      ::InternetSetStatusCallbackA(internet, new_callback);
    }
  }
}

// NOTE: this method must be called within a lock.
INTERNET_STATUS_CALLBACK WebRequestNotifier::CreateInternetStatusCallbackStub(
    INTERNET_STATUS_CALLBACK original_callback) {
  DCHECK(original_callback != NULL);

  internet_status_callback_stub_ = FunctionStub::Create(
      reinterpret_cast<uintptr_t>(original_callback),
      InternetStatusCallbackPatch);
  // internet_status_callback_stub_ is not NULL if the function stub is
  // successfully created.
  if (internet_status_callback_stub_ != NULL) {
    return reinterpret_cast<INTERNET_STATUS_CALLBACK>(
        internet_status_callback_stub_->code());
  } else {
    NOTREACHED();
    return original_callback;
  }
}

void WebRequestNotifier::HandleAfterInternetConnect(HINTERNET server,
                                                    const wchar_t* server_name,
                                                    INTERNET_PORT server_port,
                                                    DWORD service) {
  if (service != INTERNET_SERVICE_HTTP || server == NULL ||
      IS_INTRESOURCE(server_name) || wcslen(server_name) == 0) {
    return;
  }

  CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
  if (IsNotRunning())
    return;

  // It is not possible that the same connection handle is opened more than
  // once.
  DCHECK(server_map_.find(server) == server_map_.end());

  ServerInfo server_info;
  server_info.server_name = server_name;
  server_info.server_port = server_port;

  server_map_.insert(
      std::make_pair<HINTERNET, ServerInfo>(server, server_info));
}

void WebRequestNotifier::HandleAfterHttpOpenRequest(HINTERNET server,
                                                    HINTERNET request,
                                                    const char* method,
                                                    const wchar_t* path,
                                                    DWORD flags) {
  if (server == NULL || request == NULL)
    return;

  CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
  if (IsNotRunning())
    return;

  // It is not possible that the same request handle is opened more than once.
  DCHECK(request_map_.find(request) == request_map_.end());

  ServerMap::iterator server_iter = server_map_.find(server);
  // It is possible to find that we haven't recorded the connection handle to
  // the server, if we patch WinINet functions after the InternetConnect call.
  // In that case, we will ignore all events related to requests happening on
  // that connection.
  if (server_iter == server_map_.end())
    return;

  RequestInfo request_info;
  // TODO(yzshen@google.com): create the request ID.
  request_info.server_handle = server;
  request_info.method = method == NULL ? "GET" : method;
  if (!ConstructUrl((flags & INTERNET_FLAG_SECURE) != 0,
                    server_iter->second.server_name.c_str(),
                    server_iter->second.server_port,
                    path,
                    &request_info.url)) {
    NOTREACHED();
    return;
  }

  request_map_.insert(
      std::make_pair<HINTERNET, RequestInfo>(request, request_info));
}

void WebRequestNotifier::HandleBeforeHttpSendRequest(HINTERNET request) {
  CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
  if (IsNotRunning())
    return;

  RequestMap::iterator request_iter = request_map_.find(request);
  if (request_iter != request_map_.end()) {
    request_iter->second.before_request_time = base::Time::Now();
    TransitRequestToNextState(RequestInfo::WILL_NOTIFY_BEFORE_REQUEST,
                              &request_iter->second);
  }
}

void WebRequestNotifier::HandleBeforeInternetStatusCallback(
    INTERNET_STATUS_CALLBACK original,
    HINTERNET internet,
    DWORD_PTR context,
    DWORD internet_status,
    LPVOID status_information,
    DWORD status_information_length) {
  switch (internet_status) {
    case INTERNET_STATUS_HANDLE_CLOSING: {
      CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
      if (IsNotRunning())
        return;

      // We don't know whether we are closing a server or a request handle. As a
      // result, we have to test both server_map_ and request_map_.
      ServerMap::iterator server_iter = server_map_.find(internet);
      if (server_iter != server_map_.end()) {
        server_map_.erase(server_iter);
      } else {
        RequestMap::iterator request_iter = request_map_.find(internet);
        if (request_iter != request_map_.end()) {
          // TODO(yzshen@google.com): For now, we don't bother
          // checking whether the content of the response has
          // completed downloading in this case. Have to make
          // improvement if the requirement for more accurate
          // webRequest.onCompleted notifications emerges.
          TransitRequestToNextState(RequestInfo::NOTIFIED_COMPLETED,
                                    &request_iter->second);
          request_map_.erase(request_iter);
        }
      }
      break;
    }
    case INTERNET_STATUS_REQUEST_SENT: {
      CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
      if (IsNotRunning())
        return;

      RequestMap::iterator request_iter = request_map_.find(internet);
      if (request_iter != request_map_.end()) {
        TransitRequestToNextState(RequestInfo::NOTIFIED_REQUEST_SENT,
                                  &request_iter->second);
      }
      break;
    }
    case INTERNET_STATUS_REDIRECT: {
      DWORD status_code = 0;
      bool result = QueryHttpInfoNumber(internet, HTTP_QUERY_STATUS_CODE,
                                        &status_code);
      {
        CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
        if (IsNotRunning())
          return;

        RequestMap::iterator request_iter = request_map_.find(internet);
        if (request_iter != request_map_.end()) {
          RequestInfo& info = request_iter->second;
          if (result) {
            info.status_code = status_code;
            TransitRequestToNextState(RequestInfo::NOTIFIED_HEADERS_RECEIVED,
                                      &info);

            info.original_url = info.url;
            info.url = CA2W(reinterpret_cast<PCSTR>(status_information));
            TransitRequestToNextState(RequestInfo::NOTIFIED_BEFORE_REDIRECT,
                                      &info);
          } else {
            TransitRequestToNextState(RequestInfo::ERROR_OCCURRED, &info);
          }
        }
      }
      break;
    }
    case INTERNET_STATUS_REQUEST_COMPLETE: {
      DWORD status_code = 0;
      DWORD content_length = 0;
      RequestInfo::MessageLengthType length_type =
          RequestInfo::UNKNOWN_MESSAGE_LENGTH_TYPE;

      bool result = QueryHttpInfoNumber(internet, HTTP_QUERY_STATUS_CODE,
                                        &status_code) &&
                    DetermineMessageLength(internet, status_code,
                                           &content_length, &length_type);
      {
        CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
        if (IsNotRunning())
          return;

        RequestMap::iterator request_iter = request_map_.find(internet);
        if (request_iter != request_map_.end() &&
            request_iter->second.state == RequestInfo::NOTIFIED_REQUEST_SENT) {
          RequestInfo& info = request_iter->second;
          if (result) {
            info.status_code = status_code;
            info.content_length = content_length;
            info.length_type = length_type;
            TransitRequestToNextState(RequestInfo::NOTIFIED_HEADERS_RECEIVED,
                                      &info);
            if (info.length_type == RequestInfo::NO_MESSAGE_BODY)
              TransitRequestToNextState(RequestInfo::NOTIFIED_COMPLETED, &info);
          } else {
            TransitRequestToNextState(RequestInfo::ERROR_OCCURRED, &info);
          }
        }
      }
      break;
    }
    case INTERNET_STATUS_CONNECTED_TO_SERVER: {
      // TODO(yzshen@google.com): get IP information.
      break;
    }
    case INTERNET_STATUS_SENDING_REQUEST: {
      CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
      if (IsNotRunning())
        return;
      // Some HttpSendRequest() calls don't actually make HTTP requests, and the
      // corresponding request handles are closed right after the calls.
      // In that case, we ignore them totally, and don't send
      // webRequest.onBeforeRequest notifications.
      RequestMap::iterator request_iter = request_map_.find(internet);
      if (request_iter != request_map_.end() &&
          request_iter->second.state ==
          RequestInfo::WILL_NOTIFY_BEFORE_REQUEST) {
        TransitRequestToNextState(RequestInfo::NOTIFIED_BEFORE_REQUEST,
                                  &request_iter->second);
      }
      break;
    }
    default: {
      break;
    }
  }
}

void WebRequestNotifier::HandleAfterInternetReadFile(
    HINTERNET request,
    BOOL result,
    LPDWORD number_of_bytes_read) {
  if (!result || number_of_bytes_read == NULL)
    return;

  CComCritSecLock<CComAutoCriticalSection> lock(critical_section_);
  if (IsNotRunning())
    return;

  RequestMap::iterator iter = request_map_.find(request);
  if (iter != request_map_.end()) {
    RequestInfo& info = iter->second;
    // We don't update the length_type field until we reach the last request
    // of the redirection chain. As a result, the check below also prevents us
    // from firing webRequest.onCompleted before the last request in the
    // chain.
    if (info.length_type == RequestInfo::CONTENT_LENGTH_HEADER) {
      info.read_progress += *number_of_bytes_read;
      if (info.read_progress >= info.content_length &&
          info.state == RequestInfo::NOTIFIED_HEADERS_RECEIVED) {
        DCHECK(info.read_progress == info.content_length);
        TransitRequestToNextState(RequestInfo::NOTIFIED_COMPLETED, &info);
      }
    }
  }
}

// Currently this method is always called within a lock.
bool WebRequestNotifier::ConstructUrl(bool https,
                                      const wchar_t* server_name,
                                      INTERNET_PORT server_port,
                                      const wchar_t* path,
                                      std::wstring* url) {
  if (url == NULL || server_name == NULL || wcslen(server_name) == 0)
    return false;

  url->clear();
  url->append(https ? L"https://" : L"http://");
  url->append(server_name);

  bool need_port = server_port != INTERNET_INVALID_PORT_NUMBER &&
                   (https ? server_port != INTERNET_DEFAULT_HTTPS_PORT :
                            server_port != INTERNET_DEFAULT_HTTP_PORT);
  if (need_port) {
    static const int kMaxPortLength = 10;
    wchar_t buffer[kMaxPortLength];
    if (swprintf(buffer, kMaxPortLength, L":%d", server_port) == -1)
      return false;
    url->append(buffer);
  }

  url->append(path);
  return true;
}

bool WebRequestNotifier::QueryHttpInfoNumber(HINTERNET request,
                                             DWORD info_flag,
                                             DWORD* value) {
  DCHECK(value != NULL);
  *value = 0;

  DWORD size = sizeof(info_flag);
  return ::HttpQueryInfo(request, info_flag | HTTP_QUERY_FLAG_NUMBER, value,
                         &size, NULL) ? true : false;
}

bool WebRequestNotifier::DetermineMessageLength(
    HINTERNET request,
    DWORD status_code,
    DWORD* length,
    RequestInfo::MessageLengthType* type) {
  // Please see http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
  // for how the length of a message is determined.

  DCHECK(length != NULL && type != NULL);
  *length = 0;
  *type = RequestInfo::UNKNOWN_MESSAGE_LENGTH_TYPE;

  std::wstring method;
  // Request methods are case-sensitive.
  if ((status_code >= 100 && status_code < 200) ||
      status_code == 204 ||
      status_code == 304 ||
      (QueryHttpInfoString(request, HTTP_QUERY_REQUEST_METHOD, &method) &&
       method == L"HEAD")) {
    *type = RequestInfo::NO_MESSAGE_BODY;
    return true;
  }

  std::wstring transfer_encoding;
  // All transfer-coding values are case-insensitive.
  if (QueryHttpInfoString(request, HTTP_QUERY_TRANSFER_ENCODING,
                          &transfer_encoding) &&
      _wcsicmp(transfer_encoding.c_str(), L"entity") != 0) {
    *type = RequestInfo::VARIABLE_MESSAGE_LENGTH;
    return true;
  }

  DWORD content_length = 0;
  if (QueryHttpInfoNumber(request, HTTP_QUERY_CONTENT_LENGTH,
                          &content_length)) {
    *type = RequestInfo::CONTENT_LENGTH_HEADER;
    *length = content_length;
    return true;
  }

  *type = RequestInfo::VARIABLE_MESSAGE_LENGTH;
  return true;
}

bool WebRequestNotifier::QueryHttpInfoString(HINTERNET request,
                                             DWORD info_flag,
                                             std::wstring* value) {
  DCHECK(value != NULL);
  value->clear();

  DWORD size = 20;
  scoped_array<wchar_t> buffer(new wchar_t[size]);

  BOOL result = ::HttpQueryInfo(request, info_flag,
                                reinterpret_cast<LPVOID>(buffer.get()), &size,
                                NULL);
  if (!result && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    buffer.reset(new wchar_t[size]);
    result = ::HttpQueryInfo(request, info_flag,
                             reinterpret_cast<LPVOID>(buffer.get()), &size,
                             NULL);
  }
  if (!result)
    return false;

  *value = buffer.get();
  return true;
}

// NOTE: this method must be called within a lock.
void WebRequestNotifier::TransitRequestToNextState(
    RequestInfo::State next_state,
    RequestInfo* info) {
  // TODO(yzshen@google.com): generate and fill in missing parameters
  // for notifications.
  DCHECK(info != NULL);

  bool fire_on_error_occurred = false;
  switch (info->state) {
    case RequestInfo::BEGIN:
      if (next_state != RequestInfo::WILL_NOTIFY_BEFORE_REQUEST &&
          next_state != RequestInfo::ERROR_OCCURRED) {
        next_state = RequestInfo::ERROR_OCCURRED;
        // We don't fire webRequest.onErrorOccurred in this case, since the
        // first event for any request has to be webRequest.onBeforeRequest.
      }
      break;
    case RequestInfo::WILL_NOTIFY_BEFORE_REQUEST:
      if (next_state == RequestInfo::NOTIFIED_BEFORE_REQUEST) {
        webrequest_events_funnel().OnBeforeRequest(
            info->id, info->url.c_str(), info->method.c_str(), info->tab_handle,
            "other", info->before_request_time);
      } else if (next_state != RequestInfo::ERROR_OCCURRED) {
        next_state = RequestInfo::ERROR_OCCURRED;
        // We don't fire webRequest.onErrorOccurred in this case, since the
        // first event for any request has to be webRequest.onBeforeRequest.
      }
      break;
    case RequestInfo::NOTIFIED_BEFORE_REQUEST:
    case RequestInfo::NOTIFIED_BEFORE_REDIRECT:
      if (next_state == RequestInfo::NOTIFIED_REQUEST_SENT) {
        webrequest_events_funnel().OnRequestSent(
            info->id, info->url.c_str(), info->ip.c_str(), base::Time::Now());
      } else {
        if (next_state != RequestInfo::ERROR_OCCURRED)
          next_state = RequestInfo::ERROR_OCCURRED;

        fire_on_error_occurred = true;
      }
      break;
    case RequestInfo::NOTIFIED_REQUEST_SENT:
      if (next_state == RequestInfo::NOTIFIED_HEADERS_RECEIVED) {
        webrequest_events_funnel().OnHeadersReceived(
            info->id, info->url.c_str(), info->status_code, base::Time::Now());
      } else {
        if (next_state != RequestInfo::ERROR_OCCURRED)
          next_state = RequestInfo::ERROR_OCCURRED;

        fire_on_error_occurred = true;
      }
      break;
    case RequestInfo::NOTIFIED_HEADERS_RECEIVED:
      if (next_state == RequestInfo::NOTIFIED_BEFORE_REDIRECT) {
        webrequest_events_funnel().OnBeforeRedirect(
            info->id, info->original_url.c_str(), info->status_code,
            info->url.c_str(), base::Time::Now());
      } else if (next_state == RequestInfo::NOTIFIED_COMPLETED) {
        webrequest_events_funnel().OnCompleted(
            info->id, info->url.c_str(), info->status_code, base::Time::Now());
      } else {
        if (next_state != RequestInfo::ERROR_OCCURRED)
          next_state = RequestInfo::ERROR_OCCURRED;

        fire_on_error_occurred = true;
      }
      break;
    case RequestInfo::NOTIFIED_COMPLETED:
      // The webRequest.onCompleted notification is supposed to be the last
      // event sent for a given request. As a result, if the request is already
      // in the NOTIFIED_COMPLETED state, we just keep it in that state without
      // sending any further notification.
      //
      // When a request handle is closed, we consider transiting the state to
      // NOTIFIED_COMPLETED. If there is no response body or we have completed
      // reading the response body, the request has already been in this state.
      // In that case, we will hit this code path.
      next_state = RequestInfo::NOTIFIED_COMPLETED;
      break;
    case RequestInfo::ERROR_OCCURRED:
      next_state = RequestInfo::ERROR_OCCURRED;
      break;
    default:
      NOTREACHED();
      break;
  }

  if (fire_on_error_occurred) {
    webrequest_events_funnel().OnErrorOccurred(
        info->id, info->url.c_str(), L"", base::Time::Now());
  }
  info->state = next_state;
}
