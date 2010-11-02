// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Web request notifier implementation.
#ifndef CEEE_IE_PLUGIN_BHO_WEBREQUEST_NOTIFIER_H_
#define CEEE_IE_PLUGIN_BHO_WEBREQUEST_NOTIFIER_H_

#include <atlbase.h>
#include <wininet.h>

#include <map>
#include <string>

#include "app/win/iat_patch_function.h"
#include "base/singleton.h"
#include "ceee/ie/plugin/bho/webrequest_events_funnel.h"
#include "toolband.h"

struct FunctionStub;

// WebRequestNotifier monitors HTTP request/response events via WinINet hooks,
// and sends the events to the broker.
class WebRequestNotifier {
 public:
  // Starts the service if it hasn't been started.
  // @return Returns true if the service is being initialized or has been
  //         successfully initialized.
  bool RequestToStart();
  // Stops the service if it is currently running and nobody is interested in
  // the service any more. Every call to RequestToStart (even if it failed)
  // should be paired with a call to RequestToStop.
  void RequestToStop();

 protected:
  // Information related to an Internet connection.
  struct ServerInfo {
    ServerInfo() : server_port(INTERNET_INVALID_PORT_NUMBER) {}

    // The host name of the server.
    std::wstring server_name;
    // The port number on the server.
    INTERNET_PORT server_port;
  };

  // Information related to a HTTP request.
  struct RequestInfo {
    RequestInfo()
        : server_handle(NULL),
          id(-1),
          tab_handle(reinterpret_cast<CeeeWindowHandle>(INVALID_HANDLE_VALUE)),
          status_code(0),
          state(BEGIN),
          content_length(0),
          read_progress(0),
          length_type(UNKNOWN_MESSAGE_LENGTH_TYPE) {
    }

    enum State {
      // The start state.
      // Possible next state: WILL_NOTIFY_BEFORE_REQUEST;
      //                      ERROR_OCCURRED.
      BEGIN = 0,
      // We are about to fire webRequest.onBeforeRequest.
      // Possible next state: NOTIFIED_BEFORE_REQUEST;
      //                      ERROR_OCCURRED.
      WILL_NOTIFY_BEFORE_REQUEST = 1,
      // The last event fired is webRequest.onBeforeRequest.
      // Possible next state: NOTIFIED_REQUEST_SENT;
      //                      ERROR_OCCURRED.
      NOTIFIED_BEFORE_REQUEST = 2,
      // The last event fired is webRequest.onRequestSent.
      // Possible next state: NOTIFIED_HEADERS_RECEIVED;
      //                      ERROR_OCCURRED.
      NOTIFIED_REQUEST_SENT = 3,
      // The last event fired is webRequest.onHeadersReceived.
      // Possible next state: NOTIFIED_BEFORE_REDIRECT;
      //                      NOTIFIED_COMPLETED;
      //                      ERROR_OCCURRED.
      NOTIFIED_HEADERS_RECEIVED = 4,
      // The last event fired is webRequest.onBeforeRedirect.
      // Possible next state: NOTIFIED_REQUEST_SENT;
      //                      ERROR_OCCURRED.
      NOTIFIED_BEFORE_REDIRECT = 5,
      // One of the two stop states.
      // The last event fired is webRequest.onCompleted.
      // Possible next state: NOTIFIED_COMPLETED.
      NOTIFIED_COMPLETED = 6,
      // One of the two stop states.
      // Some error has occurred.
      // Possible next state: ERROR_OCCURRED.
      ERROR_OCCURRED = 7
    };

    // How the length of a message body is decided.
    enum MessageLengthType {
      UNKNOWN_MESSAGE_LENGTH_TYPE = 0,
      // There is no message body.
      NO_MESSAGE_BODY = 1,
      // The length is decided by Content-Length header.
      CONTENT_LENGTH_HEADER = 2,
      // Including:
      // (1) a Transfer-Encoding header field is present and has any value
      //     other than "identity";
      // (2) the message uses the media type "multipart/byteranges";
      // (3) the length should be decided by the server closing the connection.
      VARIABLE_MESSAGE_LENGTH = 3,
    };

    // The handle of the connection to the server.
    HINTERNET server_handle;
    // The request ID, which is unique within a browser session.
    int id;
    // The window handle of the tab which sent the request.
    CeeeWindowHandle tab_handle;
    // The standard HTTP method, such as "GET" or "POST".
    std::string method;
    // The URL to retrieve.
    std::wstring url;
    // The URL before redirection.
    std::wstring original_url;
    // The server IP.
    std::string ip;
    // The standard HTTP status code returned by the server.
    DWORD status_code;
    // The request state to help decide what event should be sent next.
    State state;
    // When the browser was about to make the request.
    base::Time before_request_time;
    // The length of the response body. It is meaningful only when length_type
    // is set to CONTENT_LENGTH_HEADER.
    DWORD content_length;
    // The progress of reading the response body.
    DWORD read_progress;
    // How the length of the response body is decided.
    MessageLengthType length_type;
  };

  WebRequestNotifier();
  virtual ~WebRequestNotifier();

  // Accessor so that we can mock it in unit tests.
  // Currently this method is always called within a lock.
  virtual WebRequestEventsFunnel& webrequest_events_funnel() {
    return webrequest_events_funnel_;
  }

  // Gets called before calling InternetSetStatusCallback.
  // @param internet The handle for which the callback is set.
  // @param callback The real callback function.
  // @return A patch for the callback function.
  INTERNET_STATUS_CALLBACK HandleBeforeInternetSetStatusCallback(
      HINTERNET internet,
      INTERNET_STATUS_CALLBACK callback);

  // Gets called before calling InternetConnect.
  // @param internet Handle returned by InternetOpen.
  void HandleBeforeInternetConnect(HINTERNET internet);

  // Gets called after calling InternetConnect.
  // @param server The sever handle returned by InternetConnect.
  // @param server_name The host name of the server.
  // @param server_port The port number on the server.
  // @param service Type of service to access.
  void HandleAfterInternetConnect(HINTERNET server,
                                  const wchar_t* server_name,
                                  INTERNET_PORT server_port,
                                  DWORD service);

  // Gets called after calling HttpOpenRequest.
  // @param server The server handle.
  // @param request The request handle.
  // @param method Standard HTTP method, such as "GET" or "POST".
  // @param path The path to the target object.
  // @param flags Internet options that are passed into HttpOpenRequest.
  void HandleAfterHttpOpenRequest(HINTERNET server,
                                  HINTERNET request,
                                  const char* method,
                                  const wchar_t* path,
                                  DWORD flags);

  // Gets called before calling HttpSendRequest.
  // @param request The request handle.
  void HandleBeforeHttpSendRequest(HINTERNET request);

  // Gets called before calling InternetStatusCallback.
  // @param original The original status callback function.
  // @param internet The handle for which the callback function is called.
  // @param context The application-defined context value associated with
  //        the internet parameter.
  // @param internet_status A status code that indicates why the callback
  //        function is called.
  // @param status_information A pointer to additional status information.
  // @param status_information_length The size, in bytes, of the additional
  //        status information.
  void HandleBeforeInternetStatusCallback(INTERNET_STATUS_CALLBACK original,
                                          HINTERNET internet,
                                          DWORD_PTR context,
                                          DWORD internet_status,
                                          LPVOID status_information,
                                          DWORD status_information_length);

  // Gets called after calling InternetReadFile.
  // @param request The request handle.
  // @param result Whether the read operation is successful or not.
  // @param number_of_bytes_read How many bytes have been read.
  void HandleAfterInternetReadFile(HINTERNET request,
                                   BOOL result,
                                   LPDWORD number_of_bytes_read);

  // InternetSetStatusCallback function patches.
  // InternetSetStatusCallback documentation can be found at:
  // http://msdn.microsoft.com/en-us/library/aa385120(VS.85).aspx
  static INTERNET_STATUS_CALLBACK STDAPICALLTYPE
      InternetSetStatusCallbackAPatch(
          HINTERNET internet,
          INTERNET_STATUS_CALLBACK internet_callback);
  static INTERNET_STATUS_CALLBACK STDAPICALLTYPE
      InternetSetStatusCallbackWPatch(
          HINTERNET internet,
          INTERNET_STATUS_CALLBACK internet_callback);

  // InternetConnect function patches.
  // InternetConnect documentation can be found at:
  // http://msdn.microsoft.com/en-us/library/aa384363(VS.85).aspx
  static HINTERNET STDAPICALLTYPE InternetConnectAPatch(
      HINTERNET internet,
      LPCSTR server_name,
      INTERNET_PORT server_port,
      LPCSTR user_name,
      LPCSTR password,
      DWORD service,
      DWORD flags,
      DWORD_PTR context);
  static HINTERNET STDAPICALLTYPE InternetConnectWPatch(
      HINTERNET internet,
      LPCWSTR server_name,
      INTERNET_PORT server_port,
      LPCWSTR user_name,
      LPCWSTR password,
      DWORD service,
      DWORD flags,
      DWORD_PTR context);

  // HttpOpenRequest function patches.
  // HttpOpenRequest documentation can be found at:
  // http://msdn.microsoft.com/en-us/library/aa384233(v=VS.85).aspx
  static HINTERNET STDAPICALLTYPE HttpOpenRequestAPatch(HINTERNET connect,
                                                        LPCSTR verb,
                                                        LPCSTR object_name,
                                                        LPCSTR version,
                                                        LPCSTR referrer,
                                                        LPCSTR* accept_types,
                                                        DWORD flags,
                                                        DWORD_PTR context);
  static HINTERNET STDAPICALLTYPE HttpOpenRequestWPatch(HINTERNET connect,
                                                        LPCWSTR verb,
                                                        LPCWSTR object_name,
                                                        LPCWSTR version,
                                                        LPCWSTR referrer,
                                                        LPCWSTR* accept_types,
                                                        DWORD flags,
                                                        DWORD_PTR context);

  // HttpSendRequest function patches.
  // HttpSendRequest documentation can be found at:
  // http://msdn.microsoft.com/en-us/library/aa384247(v=VS.85).aspx
  static BOOL STDAPICALLTYPE HttpSendRequestAPatch(HINTERNET request,
                                                   LPCSTR headers,
                                                   DWORD headers_length,
                                                   LPVOID optional,
                                                   DWORD optional_length);
  static BOOL STDAPICALLTYPE HttpSendRequestWPatch(HINTERNET request,
                                                   LPCWSTR headers,
                                                   DWORD headers_length,
                                                   LPVOID optional,
                                                   DWORD optional_length);

  // InternetStatusCallback function patch.
  // InternetStatusCallback function documentation can be found at:
  // http://msdn.microsoft.com/en-us/library/aa385121(v=VS.85).aspx
  static void CALLBACK InternetStatusCallbackPatch(
      INTERNET_STATUS_CALLBACK original,
      HINTERNET internet,
      DWORD_PTR context,
      DWORD internet_status,
      LPVOID status_information,
      DWORD status_information_length);

  // InternetReadFile function patch.
  // InternetReadFile documentation can be found at:
  // http://msdn.microsoft.com/en-us/library/aa385103(v=VS.85).aspx
  static BOOL STDAPICALLTYPE InternetReadFilePatch(
      HINTERNET file,
      LPVOID buffer,
      DWORD number_of_bytes_to_read,
      LPDWORD number_of_bytes_read);

  // Patches a WinINet function.
  // @param name The name of the function to be intercepted.
  // @param patch_function The patching helper. You could check the is_patched
  //        member of this object to see whether the patching operation is
  //        successful or not.
  // @param handler The new function implementation.
  void PatchWinINetFunction(const char* name,
                            app::win::IATPatchFunction* patch_function,
                            void* handler);

  // Constructs a URL. The method omits the port number if it is the default
  // number for the protocol, or it is INTERNET_INVALID_PORT_NUMBER.
  // Currently this method is always called within a lock.
  // @param https Is it http or https?
  // @param server_name The host name of the server.
  // @param server_port The port number on the server.
  // @param path The path to the target object.
  // @param url The returned URL.
  // @return Whether the operation is successful or not.
  bool ConstructUrl(bool https,
                    const wchar_t* server_name,
                    INTERNET_PORT server_port,
                    const wchar_t* path,
                    std::wstring* url);

  // Retrieves header information as a number.
  // Make the method virtual so that we could mock it for unit tests.
  // @param request The request handle.
  // @param info_flag Query info flags could be found on:
  //        http://msdn.microsoft.com/en-us/library/aa385351(v=VS.85).aspx
  // @param value The returned value.
  // @return Whether the operation is successful or not.
  virtual bool QueryHttpInfoNumber(HINTERNET request,
                                   DWORD info_flag,
                                   DWORD* value);
  // Retrieves header information as a string.
  // Make the method virtual so that we could mock it for unit tests.
  // @param request The request handle.
  // @param info_flag Query info flags could be found on:
  //        http://msdn.microsoft.com/en-us/library/aa385351(v=VS.85).aspx
  // @param value The returned value.
  // @return Whether the operation is successful or not.
  virtual bool QueryHttpInfoString(HINTERNET request,
                                   DWORD info_flag,
                                   std::wstring* value);

  // Determines the length of the response body.
  // @param request The request handle.
  // @param status_code Standard HTTP status code.
  // @param length Returns the length of the response body. It is meaningful
  //        only when type is set to CONTENT_LENGTH_HEADER.
  // @param type Returns how the length of the response body is decided.
  // @return Whether the operation is successful or not.
  bool DetermineMessageLength(HINTERNET request,
                              DWORD status_code,
                              DWORD* length,
                              RequestInfo::MessageLengthType* type);

  // Performs state transition on a request.
  // NOTE: this method must be called within a lock.
  // @param state The target state. Please note that if it is not a valid
  //        transition, the request may end up with a state other than the
  //        target state.
  // @param info Information about the request.
  void TransitRequestToNextState(RequestInfo::State state, RequestInfo* info);

  // Creates a function stub for the status callback function.
  // NOTE: this method must be called within a lock.
  // @param original_callback The original callback function.
  // @return A patch for the callback function.
  INTERNET_STATUS_CALLBACK CreateInternetStatusCallbackStub(
      INTERNET_STATUS_CALLBACK original_callback);

  // NOTE: this method must be called within a lock.
  // @return Returns true if the service is not functioning, either because
  //         nobody is interested in the service or because the initialization
  //         has failed.
  bool IsNotRunning() const {
    return start_count_ == 0 || initialize_state_ != SUCCEEDED_TO_INITIALIZE;
  }

  // Returns true if exactly one (but not both) of the patches has been
  // successfully applied.
  // @param patch_function_1 A function patch.
  // @param patch_function_2 Another function patch.
  // @return Returns true if exactly one of them has been successfully applied.
  bool HasPatchedOneVersion(
      const app::win::IATPatchFunction& patch_function_1,
      const app::win::IATPatchFunction& patch_function_2) const {
    return (patch_function_1.is_patched() && !patch_function_2.is_patched()) ||
           (!patch_function_1.is_patched() && patch_function_2.is_patched());
  }

  // Function patches that allow us to intercept WinINet functions.
  app::win::IATPatchFunction internet_set_status_callback_a_patch_;
  app::win::IATPatchFunction internet_set_status_callback_w_patch_;
  app::win::IATPatchFunction internet_connect_a_patch_;
  app::win::IATPatchFunction internet_connect_w_patch_;
  app::win::IATPatchFunction http_open_request_a_patch_;
  app::win::IATPatchFunction http_open_request_w_patch_;
  app::win::IATPatchFunction http_send_request_a_patch_;
  app::win::IATPatchFunction http_send_request_w_patch_;
  app::win::IATPatchFunction internet_read_file_patch_;

  // The funnel for sending webRequest events to the broker.
  WebRequestEventsFunnel webrequest_events_funnel_;

  // Used to protect the access to all the following data members.
  CComAutoCriticalSection critical_section_;

  // Used to intercept InternetStatusCallback function, which is defined by a
  // WinINet client to observe status changes.
  FunctionStub* internet_status_callback_stub_;

  // Maps Internet connection handles to ServerInfo instances.
  typedef std::map<HINTERNET, ServerInfo> ServerMap;
  ServerMap server_map_;

  // Maps HTTP request handles to RequestInfo instances.
  typedef std::map<HINTERNET, RequestInfo> RequestMap;
  RequestMap request_map_;

  // The number of RequestToStart calls minus the number of RequestToStop calls.
  // If the number drops to 0, then the service will be stopped.
  int start_count_;

  // Indicates the progress of initialization.
  enum InitializeState {
    // Initialization hasn't been started.
    NOT_INITIALIZED,
    // Initialization is happening.
    INITIALIZING,
    // Initialization has failed.
    FAILED_TO_INITIALIZE,
    // Initialization has succeeded.
    SUCCEEDED_TO_INITIALIZE
  };
  InitializeState initialize_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestNotifier);
};

// A singleton that keeps the WebRequestNotifier used by production code.
class ProductionWebRequestNotifier
    : public WebRequestNotifier,
      public Singleton<ProductionWebRequestNotifier> {
 private:
  ProductionWebRequestNotifier() {}

  friend struct DefaultSingletonTraits<ProductionWebRequestNotifier>;
  DISALLOW_COPY_AND_ASSIGN(ProductionWebRequestNotifier);
};

#endif  // CEEE_IE_PLUGIN_BHO_WEBREQUEST_NOTIFIER_H_
