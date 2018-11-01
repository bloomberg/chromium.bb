// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frame_impl.h"

#import "base/base64.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
#include "crypto/aead.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using crypto::SymmetricKey;

namespace {
const char kFrameId[] = "1effd8f52a067c8d3a01762d3c41dfd8";

// A base64 encoded sample key.
const char kFrameKey[] = "R7lsXtR74c6R9A9k691gUQ8JAd0be+w//Lntgcbjwrc=";

// Returns a key which can be used to create a WebFrame.
std::unique_ptr<SymmetricKey> CreateKey() {
  std::string decoded_frame_key_string;
  base::Base64Decode(kFrameKey, &decoded_frame_key_string);
  return crypto::SymmetricKey::Import(crypto::SymmetricKey::Algorithm::AES,
                                      decoded_frame_key_string);
}

struct RouteMessageParameters {
  NSString* encoded_function_json = nil;
  NSString* encoded_iv = nil;
  NSString* frame_id = nil;
};

RouteMessageParameters ParametersFromFunctionCallString(
    NSString* function_call) {
  NSRange parameters_start = [function_call rangeOfString:@"("];
  NSRange parameters_end = [function_call rangeOfString:@")"];
  NSString* parameter_string = [function_call
      substringWithRange:NSMakeRange(parameters_start.location + 1,
                                     parameters_end.location -
                                         parameters_start.location - 1)];
  NSArray* parameters = [parameter_string componentsSeparatedByString:@","];

  RouteMessageParameters parsed_params;
  if (parameters.count == 3) {
    NSMutableCharacterSet* trim_characters_set =
        [NSMutableCharacterSet whitespaceCharacterSet];
    [trim_characters_set addCharactersInString:@"'"];

    parsed_params.encoded_function_json =
        [parameters[0] stringByTrimmingCharactersInSet:trim_characters_set];
    parsed_params.encoded_iv =
        [parameters[1] stringByTrimmingCharactersInSet:trim_characters_set];
    parsed_params.frame_id =
        [parameters[2] stringByTrimmingCharactersInSet:trim_characters_set];
  }
  return parsed_params;
}

}  // namespace

namespace web {

typedef web::WebTest WebFrameImplTest;

// Tests creation of a WebFrame for the main frame without an encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrame) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/true, security_origin,
                         &test_web_state);

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_TRUE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for the main frame with an encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrameWithKey) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/true, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_TRUE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for a frame which is not the main frame without
// an encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForIFrame) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_FALSE(web_frame.IsMainFrame());
  EXPECT_FALSE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for a frame which is not the main frame with an
// encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForIFrameWithKey) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_FALSE(web_frame.IsMainFrame());
  EXPECT_TRUE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests that |CallJavaScriptFunction| encrypts the message and passes it to
// __gCrWeb.message.routeMessage in the main frame.
TEST_F(WebFrameImplTest, CallJavaScriptFunction) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  std::vector<base::Value> function_params;
  function_params.push_back(base::Value("plaintextParam"));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));

  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_TRUE([last_script hasPrefix:@"__gCrWeb.message.routeMessage"]);
  // Verify the message does not contain the plaintext function name or
  // parameters.
  EXPECT_FALSE([last_script containsString:@"functionName"]);
  EXPECT_FALSE([last_script containsString:@"plaintextParam"]);

  RouteMessageParameters params = ParametersFromFunctionCallString(last_script);

  // Verify that the message is a properly base64 encoded string.
  std::string decoded_message;
  EXPECT_TRUE(base::Base64Decode(
      base::SysNSStringToUTF8(params.encoded_function_json), &decoded_message));
  // Verify the message does not contain the plaintext function name or
  // parameters.
  EXPECT_FALSE([base::SysUTF8ToNSString(decoded_message)
      containsString:@"functionName"]);
  EXPECT_FALSE([base::SysUTF8ToNSString(decoded_message)
      containsString:@"plaintextParam"]);

  std::string iv_string = base::SysNSStringToUTF8(params.encoded_iv);
  std::string decoded_iv;
  // Verify that the initialization vector is a properly base64 encoded string.
  EXPECT_TRUE(base::Base64Decode(iv_string, &decoded_iv));

  // Ensure the frame ID matches.
  EXPECT_NSEQ(base::SysUTF8ToNSString(kFrameId), params.frame_id);
}

// Tests that the WebFrame uses different initialization vectors for two
// sequential calls to |CallJavaScriptFunction|.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionUniqueInitializationVector) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  std::vector<base::Value> function_params;
  function_params.push_back(base::Value("plaintextParam"));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));

  NSString* last_script1 =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  RouteMessageParameters params1 =
      ParametersFromFunctionCallString(last_script1);

  // Call JavaScript Function again to verify that the same initialization
  // vector is not reused and that the ciphertext is different.
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  NSString* last_script2 =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  RouteMessageParameters params2 =
      ParametersFromFunctionCallString(last_script2);

  EXPECT_NSNE(params1.encoded_function_json, params2.encoded_function_json);
  EXPECT_NSNE(params1.encoded_iv, params2.encoded_iv);
}

// Tests that the WebFrame properly encodes and encrypts all parameters for
// |CallJavaScriptFunction|.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionMessageProperlyEncoded) {
  std::unique_ptr<SymmetricKey> key = CreateKey();
  const std::string key_string = key->key();
  // Use an arbitrary nonzero message id to ensure it isn't matching a zero
  // value by chance.
  const int initial_message_id = 11;

  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(std::move(key));
  web_frame.SetNextMessageId(initial_message_id);

  std::vector<base::Value> function_params;
  std::string plaintext_param("plaintextParam");
  function_params.push_back(base::Value(plaintext_param));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));

  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  RouteMessageParameters params = ParametersFromFunctionCallString(last_script);

  std::string decoded_ciphertext;
  EXPECT_TRUE(
      base::Base64Decode(base::SysNSStringToUTF8(params.encoded_function_json),
                         &decoded_ciphertext));

  std::string decoded_iv;
  EXPECT_TRUE(base::Base64Decode(base::SysNSStringToUTF8(params.encoded_iv),
                                 &decoded_iv));

  // Decrypt message
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&key_string);
  std::string plaintext;
  EXPECT_TRUE(aead.Open(decoded_ciphertext, decoded_iv,
                        /*additional_data=*/nullptr, &plaintext));

  std::unique_ptr<base::Value> parsed_result(
      base::JSONReader::Read(plaintext, false));
  EXPECT_TRUE(parsed_result.get());

  base::DictionaryValue* result_dict;
  ASSERT_TRUE(parsed_result->GetAsDictionary(&result_dict));

  int decrypted_message_id = 0;
  EXPECT_TRUE(result_dict->GetInteger("messageId", &decrypted_message_id));
  EXPECT_EQ(initial_message_id, decrypted_message_id);

  bool decrypted_respond_with_result = true;
  EXPECT_TRUE(result_dict->GetBoolean("replyWithResult",
                                      &decrypted_respond_with_result));
  EXPECT_FALSE(decrypted_respond_with_result);

  std::string decrypted_functionName;
  EXPECT_TRUE(result_dict->GetString("functionName", &decrypted_functionName));
  EXPECT_STREQ("functionName", decrypted_functionName.c_str());

  base::ListValue* decrypted_parameters;
  EXPECT_TRUE(result_dict->GetList("parameters", &decrypted_parameters));
  EXPECT_EQ(function_params.size(), decrypted_parameters->GetList().size());
  EXPECT_EQ(plaintext_param, decrypted_parameters->GetList()[0].GetString());
}

// Tests that the WebFrame properly encodes and encrypts the respondWithResult
// value when |CallJavaScriptFunction| is called with a callback.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionRespondWithResult) {
  std::unique_ptr<SymmetricKey> key = CreateKey();
  const std::string key_string = key->key();
  // Use an arbitrary nonzero message id to ensure it isn't matching a zero
  // value by chance.
  const int initial_message_id = 11;

  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(std::move(key));
  web_frame.SetNextMessageId(initial_message_id);

  std::vector<base::Value> function_params;
  std::string plaintext_param("plaintextParam");
  function_params.push_back(base::Value(plaintext_param));
  EXPECT_TRUE(web_frame.CallJavaScriptFunction(
      "functionName", function_params,
      base::BindOnce(^(const base::Value* value){
      }),
      base::TimeDelta::FromSeconds(5)));

  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  RouteMessageParameters params = ParametersFromFunctionCallString(last_script);

  std::string decoded_ciphertext;
  EXPECT_TRUE(
      base::Base64Decode(base::SysNSStringToUTF8(params.encoded_function_json),
                         &decoded_ciphertext));

  std::string decoded_iv;
  EXPECT_TRUE(base::Base64Decode(base::SysNSStringToUTF8(params.encoded_iv),
                                 &decoded_iv));

  // Decrypt message
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&key_string);
  std::string plaintext;
  EXPECT_TRUE(aead.Open(decoded_ciphertext, decoded_iv,
                        /*additional_data=*/nullptr, &plaintext));

  std::unique_ptr<base::Value> parsed_result(
      base::JSONReader::Read(plaintext, false));
  EXPECT_TRUE(parsed_result.get());

  base::DictionaryValue* result_dict;
  ASSERT_TRUE(parsed_result->GetAsDictionary(&result_dict));

  bool decrypted_respond_with_result = false;
  EXPECT_TRUE(result_dict->GetBoolean("replyWithResult",
                                      &decrypted_respond_with_result));
  EXPECT_TRUE(decrypted_respond_with_result);
}

// Tests that the WebFrame properly creates JavaScript for the main frame when
// there is no encryption key.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionMainFrameWithoutKey) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/true, security_origin,
                         &test_web_state);

  std::vector<base::Value> function_params;

  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_NSEQ(@"__gCrWeb.functionName()", last_script);

  function_params.push_back(base::Value("param1"));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\")", last_script);

  function_params.push_back(base::Value(true));
  function_params.push_back(base::Value(27));
  function_params.push_back(base::Value(3.14));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\",true,27,3.14)", last_script);
}

// Tests that the WebFrame does not create JavaScript for an iframe when there
// is no encryption key.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionIFrameFrameWithoutKey) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);

  std::vector<base::Value> function_params;
  function_params.push_back(base::Value("plaintextParam"));
  EXPECT_FALSE(
      web_frame.CallJavaScriptFunction("functionName", function_params));

  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_EQ(last_script.length, 0ul);
}

}  // namespace web
