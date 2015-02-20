// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "extensions/browser/api/vpn_provider/vpn_provider_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/test_util.h"
#include "extensions/shell/test/shell_test.h"

using extensions::api_test_utils::RunFunctionAndReturnSingleResult;
using extensions::api_test_utils::RunFunctionAndReturnError;

namespace extensions {

const char* kErrorMessages[] = {"Address CIDR sanity check failed.",
                                "DNS server IP sanity check failed.",
                                "Unauthorized access."};

struct SetParameterTestParams {
  int err_index;
  const char* address;
  const char* dns_server;
};

const SetParameterTestParams set_parameter_tests[] = {
    {0, "1+++", ""},              // + not allowed
    {0, "1", ""},                 // 3 dots and separator missing
    {0, "1..", ""},               // A dot and separator missing
    {0, "1...", ""},              // Separator missing
    {0, "1.../", ""},             // No digit after separator in address
    {1, "1.../0", ""},            // Address passes sanity check, DNS incorrect
    {1, "1.../0", "1.../"},       // DNS is not CIDR
    {2, "1.../0", "1..."},        // Okay
    {0, ".../", "..."},           // Address has no digits
    {0, "0.../", "..."},          // Address has no digits post separator
    {1, "0.../0", "..."},         // Address passes sanity check, DNS incorrect
    {2, "0.../0", "...0"},        // Okay
    {0, "1...:::/1279abe", ""},   // : not allowed for ipv4
    {0, "1.../1279abcde", ""},    // Hex not allowed after separator
    {0, "1...abcde/1279", ""},    // Hex not allowed in ipv4
    {1, "1.../1279", ""},         // Address passes sanity check, DNS incorrect
    {2, "1.../1279", "1..."},     // Okay
    {0, "1--++", ""},             // + and - not supported
    {0, "1.1.1.1", ""},           // Missing separator
    {0, "1.1.1.1/", ""},          // No digits after separator in address
    {1, "1.1.1.1/1", ""},         // Address passes sanity check, DNS incorrect
    {2, "1.1.1.1/1", "1.1.1.1"},  // Okay
    {0, "1.1.1./e", "1.1.1."},    // Hex not okay in ipv4
    {2, "1.1.1./0", "1.1.1."},    // Okay
    {1, "1.../1279", "..."},      // No digits in DNS
    {1, "1.../1279", "e..."},     // Hex not allowed in ipv4
    {2, "1.../1279", "4..."},     // Okay
};

class VpnProviderApiTest
    : public AppShellTest,
      public testing::WithParamInterface<const SetParameterTestParams*> {};

IN_PROC_BROWSER_TEST_P(VpnProviderApiTest, SetParametersFunction) {
  scoped_refptr<extensions::VpnProviderSetParametersFunction>
      set_parameter_function(
          new extensions::VpnProviderSetParametersFunction());
  scoped_refptr<Extension> empty_extension = test_util::CreateEmptyExtension();

  set_parameter_function->set_extension(empty_extension.get());
  set_parameter_function->set_has_callback(true);

  const std::string args =
      "["
      "  {"
      "    \"address\": \"%s\","
      "    \"exclusionList\": [],"
      "    \"inclusionList\": [],"
      "    \"dnsServers\": [\"%s\"]"
      "  }"
      "]";

  EXPECT_EQ(kErrorMessages[GetParam()->err_index],
            RunFunctionAndReturnError(
                set_parameter_function.get(),
                base::StringPrintf(args.c_str(), GetParam()->address,
                                   GetParam()->dns_server),
                browser_context()));
}

INSTANTIATE_TEST_CASE_P(
    SetParameterTestParams,
    VpnProviderApiTest,
    testing::Range(&set_parameter_tests[0],
                   &set_parameter_tests[arraysize(set_parameter_tests)]));

}  //  namespace extensions
