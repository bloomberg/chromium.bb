// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/enterprise_platform_keys_natives.h"

#include <string>

#include "base/values.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebCryptoNormalize.h"

namespace extensions {

namespace {

bool StringToWebCryptoOperation(const std::string& str,
                                blink::WebCryptoOperation* op) {
  if (str == "GenerateKey") {
    *op = blink::WebCryptoOperationGenerateKey;
    return true;
  }
  if (str == "Sign") {
    *op = blink::WebCryptoOperationSign;
    return true;
  }
  if (str == "Verify") {
    *op = blink::WebCryptoOperationVerify;
    return true;
  }
  return false;
}

scoped_ptr<base::DictionaryValue> WebCryptoAlgorithmToBaseValue(
    const blink::WebCryptoAlgorithm& algorithm) {
  DCHECK(!algorithm.isNull());

  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  const blink::WebCryptoAlgorithmInfo* info =
      blink::WebCryptoAlgorithm::lookupAlgorithmInfo(algorithm.id());
  dict->SetStringWithoutPathExpansion("name", info->name);
  const blink::WebCryptoRsaHashedKeyGenParams* rsaHashedKeyGen =
      algorithm.rsaHashedKeyGenParams();
  if (rsaHashedKeyGen) {
    dict->SetIntegerWithoutPathExpansion("modulusLength",
                                         rsaHashedKeyGen->modulusLengthBits());
    const blink::WebVector<unsigned char>& public_exponent =
        rsaHashedKeyGen->publicExponent();
    dict->SetWithoutPathExpansion(
        "publicExponent",
        base::BinaryValue::CreateWithCopiedBuffer(
            reinterpret_cast<const char*>(public_exponent.data()),
            public_exponent.size()));

    const blink::WebCryptoAlgorithm& hash = rsaHashedKeyGen->hash();
    DCHECK(!hash.isNull());
    const blink::WebCryptoAlgorithmInfo* hash_info =
        blink::WebCryptoAlgorithm::lookupAlgorithmInfo(hash.id());

    scoped_ptr<base::DictionaryValue> hash_dict(new base::DictionaryValue);
    hash_dict->SetStringWithoutPathExpansion("name", hash_info->name);
    dict->SetWithoutPathExpansion("hash", hash_dict.release());
  }
  // Otherwise, |algorithm| is missing support here or no parameters were
  // required.
  return dict.Pass();
}

}  // namespace

EnterprisePlatformKeysNatives::EnterprisePlatformKeysNatives(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("NormalizeAlgorithm",
                base::Bind(&EnterprisePlatformKeysNatives::NormalizeAlgorithm,
                           base::Unretained(this)));
}

void EnterprisePlatformKeysNatives::NormalizeAlgorithm(
    const v8::FunctionCallbackInfo<v8::Value>& call_info) {
  DCHECK_EQ(call_info.Length(), 2);
  DCHECK(call_info[0]->IsObject());
  DCHECK(call_info[1]->IsString());

  blink::WebCryptoOperation operation;
  if (!StringToWebCryptoOperation(*v8::String::Utf8Value(call_info[1]),
                                  &operation)) {
    return;
  }

  blink::WebString error_details;
  int exception_code = 0;

  blink::WebCryptoAlgorithm algorithm =
      blink::normalizeCryptoAlgorithm(call_info[0]->ToObject(),
                                      operation,
                                      &exception_code,
                                      &error_details,
                                      call_info.GetIsolate());

  scoped_ptr<base::DictionaryValue> algorithm_dict;
  if (!algorithm.isNull())
    algorithm_dict = WebCryptoAlgorithmToBaseValue(algorithm);

  if (!algorithm_dict)
    return;

  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  call_info.GetReturnValue().Set(
      converter->ToV8Value(algorithm_dict.get(), context()->v8_context()));
}

}  // namespace extensions
