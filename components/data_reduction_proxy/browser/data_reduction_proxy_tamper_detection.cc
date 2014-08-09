// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_tamper_detection.h"

#include <algorithm>
#include <cstring>

#include "base/base64.h"
#include "base/md5.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

// Macro for UMA reporting. HTTP response first reports to histogram events
// |http_histogram| by |carrier_id|; then reports the total counts to
// |http_histogram|_Total. HTTPS response reports to histograms
// |https_histogram| and |https_histogram|_Total similarly.
#define REPORT_TAMPER_DETECTION_UMA( \
    scheme_is_https, https_histogram, http_histogram, carrier_id) \
  do { \
    if (scheme_is_https) { \
      UMA_HISTOGRAM_SPARSE_SLOWLY(https_histogram, carrier_id); \
      UMA_HISTOGRAM_COUNTS(https_histogram "_Total", 1); \
    } else { \
      UMA_HISTOGRAM_SPARSE_SLOWLY(http_histogram, carrier_id); \
      UMA_HISTOGRAM_COUNTS(http_histogram "_Total", 1); \
    }\
  } while (0)

namespace data_reduction_proxy {

// static
bool DataReductionProxyTamperDetection::DetectAndReport(
    const net::HttpResponseHeaders* headers,
    const bool scheme_is_https) {
  DCHECK(headers);
  // Abort tamper detection, if the fingerprint of the Chrome-Proxy header is
  // absent.
  std::string chrome_proxy_fingerprint;
  if (!GetDataReductionProxyActionFingerprintChromeProxy(
      headers, &chrome_proxy_fingerprint)) {
    return false;
  }

  // Get carrier ID.
  unsigned carrier_id = 0;
#if defined(OS_ANDROID)
  base::StringToUint(net::android::GetTelephonyNetworkOperator(), &carrier_id);
#endif

  DataReductionProxyTamperDetection tamper_detection(
      headers, scheme_is_https, carrier_id);

  // Checks if the Chrome-Proxy header has been tampered with.
  if (tamper_detection.ValidateChromeProxyHeader(chrome_proxy_fingerprint)) {
    tamper_detection.ReportUMAforChromeProxyHeaderValidation();
    return true;
  }

  // Chrome-Proxy header has not been tampered with, and thus other
  // fingerprints are valid. Reports the number of responses that other
  // fingerprints will be checked.
  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https,
      "DataReductionProxy.HeaderTamperDetectionHTTPS",
      "DataReductionProxy.HeaderTamperDetectionHTTP",
      carrier_id);

  bool tampered = false;
  std::string fingerprint;

  if (GetDataReductionProxyActionFingerprintVia(headers, &fingerprint)) {
    bool has_chrome_proxy_via_header;
    if (tamper_detection.ValidateViaHeader(
        fingerprint, &has_chrome_proxy_via_header)) {
      tamper_detection.ReportUMAforViaHeaderValidation(
          has_chrome_proxy_via_header);
      tampered = true;
    }
  }

  if (GetDataReductionProxyActionFingerprintOtherHeaders(
      headers, &fingerprint)) {
    if (tamper_detection.ValidateOtherHeaders(fingerprint)) {
      tamper_detection.ReportUMAforOtherHeadersValidation();
      tampered = true;
    }
  }

  if (GetDataReductionProxyActionFingerprintContentLength(
      headers, &fingerprint)) {
    if (tamper_detection.ValidateContentLengthHeader(fingerprint)) {
      tamper_detection.ReportUMAforContentLengthHeaderValidation();
      tampered = true;
    }
  }

  if (!tampered) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https,
        "DataReductionProxy.HeaderTamperDetectionPassHTTPS",
        "DataReductionProxy.HeaderTamperDetectionPassHTTP",
        carrier_id);
  }

  return tampered;
}

// Constructor initializes the map of fingerprint names to codes.
DataReductionProxyTamperDetection::DataReductionProxyTamperDetection(
    const net::HttpResponseHeaders* headers,
    const bool is_secure,
    const unsigned carrier_id)
    : response_headers_(headers),
      scheme_is_https_(is_secure),
      carrier_id_(carrier_id) {
  DCHECK(headers);
}

DataReductionProxyTamperDetection::~DataReductionProxyTamperDetection() {};

// |fingerprint| is Base64 encoded. Decodes it first. Then calculates the
// fingerprint of received Chrome-Proxy header, and compares the two to see
// whether they are equal or not.
bool DataReductionProxyTamperDetection::ValidateChromeProxyHeader(
    const std::string& fingerprint) const {
  std::string received_fingerprint;
  if (!base::Base64Decode(fingerprint, &received_fingerprint))
    return true;

  // Gets the Chrome-Proxy header values with its fingerprint removed.
  std::vector<std::string> chrome_proxy_header_values;
  GetDataReductionProxyHeaderWithFingerprintRemoved(
      response_headers_, &chrome_proxy_header_values);

  // Calculates the MD5 hash value of Chrome-Proxy.
  std::string actual_fingerprint;
  GetMD5(ValuesToSortedString(&chrome_proxy_header_values),
         &actual_fingerprint);

  return received_fingerprint != actual_fingerprint;
}

void DataReductionProxyTamperDetection::
    ReportUMAforChromeProxyHeaderValidation() const {
  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https_,
      "DataReductionProxy.HeaderTamperedHTTPS_ChromeProxy",
      "DataReductionProxy.HeaderTamperedHTTP_ChromeProxy",
      carrier_id_);
}

// Checks whether there are other proxies/middleboxes' named after the data
// reduction proxy's name in Via header. |has_chrome_proxy_via_header| marks
// that whether the data reduction proxy's Via header occurs or not.
bool DataReductionProxyTamperDetection::ValidateViaHeader(
    const std::string& fingerprint,
    bool* has_chrome_proxy_via_header) const {
  bool has_intermediary;
  *has_chrome_proxy_via_header = HasDataReductionProxyViaHeader(
      response_headers_,
      &has_intermediary);

  if (*has_chrome_proxy_via_header)
    return !has_intermediary;
  return true;
}

void DataReductionProxyTamperDetection::ReportUMAforViaHeaderValidation(
    bool has_chrome_proxy) const {
  // The Via header of the data reduction proxy is missing.
  if (!has_chrome_proxy) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_Via_Missing",
        "DataReductionProxy.HeaderTamperedHTTP_Via_Missing",
        carrier_id_);
    return;
  }

  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https_,
      "DataReductionProxy.HeaderTamperedHTTPS_Via",
      "DataReductionProxy.HeaderTamperedHTTP_Via",
      carrier_id_);
}

// The data reduction proxy constructs a canonical representation of values of
// a list of headers. The fingerprint is constructed as follows:
// 1) for each header, gets the string representation of its values (same as
//    ValuesToSortedString);
// 2) concatenates all header's string representations using ";" as a delimiter;
// 3) calculates the MD5 hash value of above concatenated string;
// 4) appends the header names to the fingerprint, with a delimiter "|".
// The constructed fingerprint looks like:
//     [hashed_fingerprint]|header_name1|header_namer2:...
//
// To check whether such a fingerprint matches the response that the Chromium
// client receives, the client firstly extracts the header names. For
// each header, gets its string representation (by ValuesToSortedString),
// concatenates them and calculates the MD5 hash value. Compares the hash
// value to the fingerprint received from the data reduction proxy.
bool DataReductionProxyTamperDetection::ValidateOtherHeaders(
    const std::string& fingerprint) const {
  DCHECK(!fingerprint.empty());

  // According to RFC 2616, "|" is not a valid character in a header name; and
  // it is not a valid base64 encoding character, so there is no ambituity in
  //using it as a delimiter.
  net::HttpUtil::ValuesIterator it(
      fingerprint.begin(), fingerprint.end(), '|');

  // The first value is the base64 encoded fingerprint.
  std::string received_fingerprint;
  if (!it.GetNext() ||
      !base::Base64Decode(it.value(), &received_fingerprint)) {
    NOTREACHED();
    return true;
  }

  std::string header_values;
  // The following values are the header names included in the fingerprint
  // calculation.
  while (it.GetNext()) {
    // Gets values of one header.
    std::vector<std::string> response_header_values =
        GetHeaderValues(response_headers_, it.value());
    // Sorts the values and concatenate them, with delimiter ";". ";" can occur
    // in a header value and thus two different sets of header values could map
    // to the same string representation. This should be very rare.
    // TODO(xingx): find an unambiguous representation.
    header_values += ValuesToSortedString(&response_header_values)  + ";";
  }

  // Calculates the MD5 hash of the concatenated string.
  std::string actual_fingerprint;
  GetMD5(header_values, &actual_fingerprint);

  return received_fingerprint != actual_fingerprint;
}

void DataReductionProxyTamperDetection::
    ReportUMAforOtherHeadersValidation() const {
  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https_,
      "DataReductionProxy.HeaderTamperedHTTPS_OtherHeaders",
      "DataReductionProxy.HeaderTamperedHTTP_OtherHeaders",
      carrier_id_);
}

// The Content-Length value will not be reported as different if at either side
// (the data reduction proxy side and the client side), the Content-Length is
// missing or it cannot be decoded as a valid integer.
bool DataReductionProxyTamperDetection::ValidateContentLengthHeader(
    const std::string& fingerprint) const {
  int received_content_length_fingerprint, actual_content_length;
  // Abort, if Content-Length value from the data reduction proxy does not
  // exist or it cannot be converted to an integer.
  if (!base::StringToInt(fingerprint, &received_content_length_fingerprint))
    return false;

  std::string actual_content_length_string;
  // Abort, if there is no Content-Length header received.
  if (!response_headers_->GetNormalizedHeader("Content-Length",
      &actual_content_length_string)) {
    return false;
  }

  // Abort, if the Content-Length value cannot be converted to integer.
  if (!base::StringToInt(actual_content_length_string,
                         &actual_content_length)) {
    return false;
  }

  return received_content_length_fingerprint != actual_content_length;
}

void DataReductionProxyTamperDetection::
    ReportUMAforContentLengthHeaderValidation() const {
  // Gets MIME type of the response and reports to UMA histograms separately.
  // Divides MIME types into 4 groups: JavaScript, CSS, Images, and others.
  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https_,
      "DataReductionProxy.HeaderTamperedHTTPS_ContentLength",
      "DataReductionProxy.HeaderTamperedHTTP_ContentLength",
      carrier_id_);

  // Gets MIME type.
  std::string mime_type;
  response_headers_->GetMimeType(&mime_type);

  std::string JS1   = "text/javascript";
  std::string JS2   = "application/x-javascript";
  std::string JS3   = "application/javascript";
  std::string CSS   = "text/css";
  std::string IMAGE = "image/";

  size_t mime_type_size = mime_type.size();
  if ((mime_type_size >= JS1.size() && LowerCaseEqualsASCII(mime_type.begin(),
      mime_type.begin() + JS1.size(), JS1.c_str())) ||
      (mime_type_size >= JS2.size() && LowerCaseEqualsASCII(mime_type.begin(),
      mime_type.begin() + JS2.size(), JS2.c_str())) ||
      (mime_type_size >= JS3.size() && LowerCaseEqualsASCII(mime_type.begin(),
      mime_type.begin() + JS3.size(), JS3.c_str()))) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_JS",
        "DataReductionProxy.HeaderTamperedHTTP_ContentLength_JS",
        carrier_id_);
  } else if (mime_type_size >= CSS.size() &&
      LowerCaseEqualsASCII(mime_type.begin(),
      mime_type.begin() + CSS.size(), CSS.c_str())) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_CSS",
        "DataReductionProxy.HeaderTamperedHTTP_ContentLength_CSS",
        carrier_id_);
  } else if (mime_type_size >= IMAGE.size() &&
      LowerCaseEqualsASCII(mime_type.begin(),
      mime_type.begin() + IMAGE.size(), IMAGE.c_str())) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_Image",
        "DataReductionProxy.HeaderTamperedHTTP_ContentLength_Image",
        carrier_id_);
  } else {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_Other",
        "DataReductionProxy.HeaderTamperedHTTP_ContentLength_Other",
        carrier_id_);
  }
}

// We construct a canonical representation of the header so that reordered
// header values will produce the same fingerprint. The fingerprint is
// constructed as follows:
// 1) sort the values;
// 2) concatenate sorted values with a "," delimiter.
std::string DataReductionProxyTamperDetection::ValuesToSortedString(
    std::vector<std::string>* values) {
  std::string concatenated_values;
  DCHECK(values);
  if (!values) return "";

  std::sort(values->begin(), values->end());
  for (size_t i = 0; i < values->size(); ++i) {
    // Concatenates with delimiter ",".
    concatenated_values += (*values)[i] + ",";
  }
  return concatenated_values;
}

void DataReductionProxyTamperDetection::GetMD5(
    const std::string& input, std::string* output) {
  base::MD5Digest digest;
  base::MD5Sum(input.c_str(), input.size(), &digest);
  *output = std::string(
      reinterpret_cast<char*>(digest.a), ARRAYSIZE_UNSAFE(digest.a));
}

std::vector<std::string> DataReductionProxyTamperDetection::GetHeaderValues(
    const net::HttpResponseHeaders* headers,
    const std::string& header_name) {
  std::vector<std::string> values;
  std::string value;
  void* iter = NULL;
  while (headers->EnumerateHeader(&iter, header_name, &value)) {
    values.push_back(value);
  }
  return values;
}

}  // namespace data_reduction_proxy
