// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TEST_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_TEST_UTIL_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/template_util.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class GURL;

namespace base {
class FilePath;
class Value;
}

namespace google_apis {

class AboutResource;
class AccountMetadata;
class AppList;
class AuthenticatedOperationInterface;
class ResourceEntry;
class ResourceList;
struct UploadRangeResponse;

namespace test_server {
struct HttpRequest;
class HttpResponse;
}

namespace test_util {

// Runs a task posted to the blocking pool, including subsequent tasks posted
// to the UI message loop and the blocking pool.
//
// A task is often posted to the blocking pool with PostTaskAndReply(). In
// that case, a task is posted back to the UI message loop, which can again
// post a task to the blocking pool. This function processes these tasks
// repeatedly.
void RunBlockingPoolTask();

// Runs the closure, and then quits the current MessageLoop.
void RunAndQuit(const base::Closure& closure);

// Removes |prefix| from |input| and stores the result in |output|. Returns
// true if the prefix is removed.
bool RemovePrefix(const std::string& input,
                  const std::string& prefix,
                  std::string* output);

// Returns the absolute path for a test file stored under
// chrome/test/data.
base::FilePath GetTestFilePath(const std::string& relative_path);

// Returns the base URL for communicating with the local test server for
// testing, running at the specified port number.
GURL GetBaseUrlForTesting(int port);

// Loads a test JSON file as a base::Value, from a test file stored under
// chrome/test/data.
scoped_ptr<base::Value> LoadJSONFile(const std::string& relative_path);

// Copies the results from ResumeUploadCallback.
void CopyResultsFromUploadRangeCallback(
    UploadRangeResponse* response_out,
    scoped_ptr<ResourceEntry>* entry_out,
    const UploadRangeResponse& response_in,
    scoped_ptr<ResourceEntry> entry_in);

// Returns a HttpResponse created from the given file path.
scoped_ptr<test_server::HttpResponse> CreateHttpResponseFromFile(
    const base::FilePath& file_path);

// Does nothing for ReAuthenticateCallback(). This function should be used
// if it is not expected to reach this method as there won't be any
// authentication failures in the test.
void DoNothingForReAuthenticateCallback(
    AuthenticatedOperationInterface* operation);

// Handles a request for downloading a file. Reads a file from the test
// directory and returns the content. Also, copies the |request| to the memory
// pointed by |out_request|.
// |base_url| must be set to the server's base url.
scoped_ptr<test_server::HttpResponse> HandleDownloadRequest(
    const GURL& base_url,
    test_server::HttpRequest* out_request,
    const test_server::HttpRequest& request);

// Returns true if |json_data| is not NULL and equals to the content in
// |expected_json_file_path|. The failure reason will be logged into LOG(ERROR)
// if necessary.
bool VerifyJsonData(const base::FilePath& expected_json_file_path,
                    const base::Value* json_data);

// Parses a value of Content-Range header, which looks like
// "bytes <start_position>-<end_position>/<length>".
// Returns true on success.
bool ParseContentRangeHeader(const std::string& value,
                             int64* start_position,
                             int64* end_position,
                             int64* length);

// Google API related code and Drive File System code work on asynchronous
// architecture and return the results via callbacks.
// Following code implements a callback to copy such results.
// Here is how to use:
//
//   // Prepare result storage.
//   ResultType1 result1;
//   ResultType2 result2;
//           :
//
//   PerformAsynchronousTask(
//       param1, param2, ...,
//       CreateCopyResultCallback(&result1, &result2, ...));
//   RunBlockingPoolTask();  // Run message loop to complete the async task.
//
//   // Hereafter, we can write expectation with results.
//   EXPECT_EQ(expected_result1, result1);
//   EXPECT_EQ(expected_result2, result2);
//                     :
//
// Note: The max arity of the supported function is 3. The limitation comes
//   from the max arity of base::Callback, which is 7. A created callback
//   consumes two arguments for each input type.
// TODO(hidehiko): Use replace CopyResultFromXxxCallback method defined above
//   by this one. (crbug.com/180569).
namespace internal {
// Following helper templates are to support Chrome's move semantics.
// Their goal is defining helper methods which are similar to:
//   void CopyResultCallback1(T1* out1, T1&& in1)
//   void CopyResultCallback2(T1* out1, T2* out2, T1&& in1, T2&& in2)
//            :
// in C++11.

// Declare if the type is movable or not. Currently limited to scoped_ptr only.
// We can add more types upon the usage.
template<typename T> struct IsMovable : base::false_type {};
template<typename T, typename D>
struct IsMovable<scoped_ptr<T, D> > : base::true_type {};

// InType is const T& if |UseConstRef| is true, otherwise |T|.
template<bool UseConstRef, typename T> struct InTypeHelper {
  typedef const T& InType;
};
template<typename T> struct InTypeHelper<false, T> {
  typedef T InType;
};

// Simulates the std::move operation in C++11. We use pointer here for argument,
// instead of rvalue reference.
template<bool IsMovable, typename T> struct MoveHelper {
  static const T& Move(const T* in) { return *in; }
};
template<typename T> struct MoveHelper<true, T> {
  static T Move(T* in) { return in->Pass(); }
};

// Helper to handle Chrome's move semantics correctly.
template<typename T>
struct CopyResultCallbackHelper
      // It is necessary to calculate the exact signature of callbacks we want
      // to create here. In our case, as we use value-parameters for primitive
      // types and movable types in the callback declaration.
      // Thus the incoming type is as follows:
      // 1) If the argument type |T| is class type but doesn't movable,
      //    |InType| is const T&.
      // 2) Otherwise, |T| as is.
    : InTypeHelper<
          base::is_class<T>::value && !IsMovable<T>::value,  // UseConstRef
          T>,
      MoveHelper<IsMovable<T>::value, T> {
};

// Copies the |in|'s value to |out|.
template<typename T1>
void CopyResultCallback(
    T1* out,
    typename CopyResultCallbackHelper<T1>::InType in) {
  *out = CopyResultCallbackHelper<T1>::Move(&in);
}

// Copies the |in1|'s value to |out1|, and |in2|'s to |out2|.
template<typename T1, typename T2>
void CopyResultCallback(
    T1* out1,
    T2* out2,
    typename CopyResultCallbackHelper<T1>::InType in1,
    typename CopyResultCallbackHelper<T2>::InType in2) {
  *out1 = CopyResultCallbackHelper<T1>::Move(&in1);
  *out2 = CopyResultCallbackHelper<T2>::Move(&in2);
}

// Copies the |in1|'s value to |out1|, |in2|'s to |out2|, and |in3|'s to |out3|.
template<typename T1, typename T2, typename T3>
void CopyResultCallback(
    T1* out1,
    T2* out2,
    T3* out3,
    typename CopyResultCallbackHelper<T1>::InType in1,
    typename CopyResultCallbackHelper<T2>::InType in2,
    typename CopyResultCallbackHelper<T3>::InType in3) {
  *out1 = CopyResultCallbackHelper<T1>::Move(&in1);
  *out2 = CopyResultCallbackHelper<T2>::Move(&in2);
  *out3 = CopyResultCallbackHelper<T3>::Move(&in3);
}

}  // namespace internal

template<typename T1>
base::Callback<void(typename internal::CopyResultCallbackHelper<T1>::InType)>
CreateCopyResultCallback(T1* out1) {
  return base::Bind(&internal::CopyResultCallback<T1>, out1);
}

template<typename T1, typename T2>
base::Callback<void(typename internal::CopyResultCallbackHelper<T1>::InType,
                    typename internal::CopyResultCallbackHelper<T2>::InType)>
CreateCopyResultCallback(T1* out1, T2* out2) {
  return base::Bind(&internal::CopyResultCallback<T1, T2>, out1, out2);
}

template<typename T1, typename T2, typename T3>
base::Callback<void(typename internal::CopyResultCallbackHelper<T1>::InType,
                    typename internal::CopyResultCallbackHelper<T2>::InType,
                    typename internal::CopyResultCallbackHelper<T3>::InType)>
CreateCopyResultCallback(T1* out1, T2* out2, T3* out3) {
  return base::Bind(
      &internal::CopyResultCallback<T1, T2, T3>, out1, out2, out3);
}

}  // namespace test_util
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_TEST_UTIL_H_
