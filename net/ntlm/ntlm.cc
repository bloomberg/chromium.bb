// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ntlm/ntlm.h"

#include <string.h>

#include "base/logging.h"
#include "base/md5.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_string_util.h"
#include "net/ntlm/des.h"
#include "net/ntlm/md4.h"
#include "net/ntlm/ntlm_buffer_writer.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"

namespace net {
namespace ntlm {

namespace {

// Takes the parsed target info in |av_pairs| and performs the following
// actions.
//
// 1) If a |TargetInfoAvId::kTimestamp| AvPair exists, |server_timestamp|
//    is set to the payload.
// 2) If |is_mic_enabled| is true, the existing |TargetInfoAvId::kFlags| AvPair
//    will have the |TargetInfoAvFlags::kMicPresent| bit set. If an existing
//    flags AvPair does not already exist, a new one is added with the value of
//    |TargetInfoAvFlags::kMicPresent|.
// 3) If |is_epa_enabled| is true, two new AvPair entries will be added to
//    |av_pairs|. The first will be of type |TargetInfoAvId::kChannelBindings|
//    and contains MD5(|channel_bindings|) as the payload. The second will be
//    of type |TargetInfoAvId::kTargetName| and contains |spn| as a little
//    endian UTF16 string.
// 4) Sets |target_info_len| to the size of |av_pairs| when serialized into
//    a payload.
void UpdateTargetInfoAvPairs(bool is_mic_enabled,
                             bool is_epa_enabled,
                             const std::string& channel_bindings,
                             const std::string& spn,
                             std::vector<AvPair>* av_pairs,
                             uint64_t* server_timestamp,
                             size_t* target_info_len) {
  // Do a pass to update flags and calculate current length and
  // pull out the server timestamp if it is there.
  *server_timestamp = UINT64_MAX;
  *target_info_len = 0;

  bool need_flags_added = is_mic_enabled;
  for (AvPair& pair : *av_pairs) {
    *target_info_len += pair.avlen + kAvPairHeaderLen;
    switch (pair.avid) {
      case TargetInfoAvId::kFlags:
        // The parsing phase already set the payload to the |flags| field.
        if (is_mic_enabled) {
          pair.flags = pair.flags | TargetInfoAvFlags::kMicPresent;
        }

        need_flags_added = false;
        break;
      case TargetInfoAvId::kTimestamp:
        // The parsing phase already set the payload to the |timestamp| field.
        *server_timestamp = pair.timestamp;
        break;
      case TargetInfoAvId::kEol:
      case TargetInfoAvId::kChannelBindings:
      case TargetInfoAvId::kTargetName:
        // The terminator, |kEol|, should already have been removed from the
        // end of the list and would have been rejected if it has been inside
        // the list. Additionally |kChannelBindings| and |kTargetName| pairs
        // would have been rejected during the initial parsing. See
        // |NtlmBufferReader::ReadTargetInfo|.
        NOTREACHED();
        break;
      default:
        // Ignore entries we don't care about.
        break;
    }
  }

  if (need_flags_added) {
    DCHECK(is_mic_enabled);
    AvPair flags_pair(TargetInfoAvId::kFlags, sizeof(uint32_t));
    flags_pair.flags = TargetInfoAvFlags::kMicPresent;

    av_pairs->push_back(flags_pair);
    *target_info_len += kAvPairHeaderLen + flags_pair.avlen;
  }

  if (is_epa_enabled) {
    if (channel_bindings.empty()) {
      // When no channel bindings are supplied, the channel binding hash is
      // set to all zeros.
      av_pairs->emplace_back(TargetInfoAvId::kChannelBindings,
                             Buffer(kChannelBindingsHashLen, 0));
    } else {
      // Hash the channel bindings.
      base::MD5Digest channel_bindings_hash;
      GenerateChannelBindingHashV2(channel_bindings, &channel_bindings_hash);
      av_pairs->emplace_back(
          TargetInfoAvId::kChannelBindings,
          Buffer(channel_bindings_hash.a, kChannelBindingsHashLen));
    }

    // Convert the SPN to little endian unicode.
    base::string16 spn16 = base::UTF8ToUTF16(spn);
    NtlmBufferWriter spn_writer(spn16.length() * 2);
    bool spn_writer_result =
        spn_writer.WriteUtf16String(spn16) && spn_writer.IsEndOfBuffer();
    DCHECK(spn_writer_result);

    av_pairs->emplace_back(TargetInfoAvId::kTargetName, spn_writer.Pass());

    // Add the length of the two new AV Pairs to the total length.
    *target_info_len +=
        (2 * kAvPairHeaderLen) + kChannelBindingsHashLen + (spn16.length() * 2);
  }

  // Add extra space for the terminator at the end.
  *target_info_len += kAvPairHeaderLen;
}

Buffer WriteUpdatedTargetInfo(const std::vector<AvPair>& av_pairs,
                              size_t updated_target_info_len) {
  bool result = true;
  NtlmBufferWriter writer(updated_target_info_len);
  for (const AvPair& pair : av_pairs) {
    result = writer.WriteAvPair(pair);
    DCHECK(result);
  }

  result = writer.WriteAvPairTerminator() && writer.IsEndOfBuffer();
  DCHECK(result);
  return writer.Pass();
}

}  // namespace

void GenerateNtlmHashV1(const base::string16& password, uint8_t* hash) {
  size_t length = password.length() * 2;
  NtlmBufferWriter writer(length);

  // The writer will handle the big endian case if necessary.
  bool result = writer.WriteUtf16String(password);
  DCHECK(result);

  weak_crypto::MD4Sum(
      reinterpret_cast<const uint8_t*>(writer.GetBuffer().data()), length,
      hash);
}

void GenerateResponseDesl(const uint8_t* hash,
                          const uint8_t* challenge,
                          uint8_t* response) {
  // See DESL(K, D) function in [MS-NLMP] Section 6
  uint8_t key1[8];
  uint8_t key2[8];
  uint8_t key3[8];

  // The last 2 bytes of the hash are zero padded (5 zeros) as the
  // input to generate key3.
  uint8_t padded_hash[7];
  padded_hash[0] = hash[14];
  padded_hash[1] = hash[15];
  memset(padded_hash + 2, 0, 5);

  DESMakeKey(hash, key1);
  DESMakeKey(hash + 7, key2);
  DESMakeKey(padded_hash, key3);

  DESEncrypt(key1, challenge, response);
  DESEncrypt(key2, challenge, response + 8);
  DESEncrypt(key3, challenge, response + 16);
}

void GenerateNtlmResponseV1(const base::string16& password,
                            const uint8_t* challenge,
                            uint8_t* ntlm_response) {
  uint8_t ntlm_hash[kNtlmHashLen];
  GenerateNtlmHashV1(password, ntlm_hash);
  GenerateResponseDesl(ntlm_hash, challenge, ntlm_response);
}

void GenerateResponsesV1(const base::string16& password,
                         const uint8_t* server_challenge,
                         uint8_t* lm_response,
                         uint8_t* ntlm_response) {
  GenerateNtlmResponseV1(password, server_challenge, ntlm_response);

  // In NTLM v1 (with LMv1 disabled), the lm_response and ntlm_response are the
  // same. So just copy the ntlm_response into the lm_response.
  memcpy(lm_response, ntlm_response, kResponseLenV1);
}

void GenerateLMResponseV1WithSessionSecurity(const uint8_t* client_challenge,
                                             uint8_t* lm_response) {
  // In NTLM v1 with Session Security (aka NTLM2) the lm_response is 8 bytes of
  // client challenge and 16 bytes of zeros. (See 3.3.1)
  memcpy(lm_response, client_challenge, kChallengeLen);
  memset(lm_response + kChallengeLen, 0, kResponseLenV1 - kChallengeLen);
}

void GenerateSessionHashV1WithSessionSecurity(const uint8_t* server_challenge,
                                              const uint8_t* client_challenge,
                                              base::MD5Digest* session_hash) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(
      &ctx, base::StringPiece(reinterpret_cast<const char*>(server_challenge),
                              kChallengeLen));
  base::MD5Update(
      &ctx, base::StringPiece(reinterpret_cast<const char*>(client_challenge),
                              kChallengeLen));

  base::MD5Final(session_hash, &ctx);
}

void GenerateNtlmResponseV1WithSessionSecurity(const base::string16& password,
                                               const uint8_t* server_challenge,
                                               const uint8_t* client_challenge,
                                               uint8_t* ntlm_response) {
  // Generate the NTLMv1 Hash.
  uint8_t ntlm_hash[kNtlmHashLen];
  GenerateNtlmHashV1(password, ntlm_hash);

  // Generate the NTLMv1 Session Hash.
  base::MD5Digest session_hash;
  GenerateSessionHashV1WithSessionSecurity(server_challenge, client_challenge,
                                           &session_hash);

  // Only the first 8 bytes of |session_hash.a| are actually used.
  GenerateResponseDesl(ntlm_hash, session_hash.a, ntlm_response);
}

void GenerateResponsesV1WithSessionSecurity(const base::string16& password,
                                            const uint8_t* server_challenge,
                                            const uint8_t* client_challenge,
                                            uint8_t* lm_response,
                                            uint8_t* ntlm_response) {
  GenerateLMResponseV1WithSessionSecurity(client_challenge, lm_response);
  GenerateNtlmResponseV1WithSessionSecurity(password, server_challenge,
                                            client_challenge, ntlm_response);
}

void GenerateNtlmHashV2(const base::string16& domain,
                        const base::string16& username,
                        const base::string16& password,
                        uint8_t* v2_hash) {
  // NOTE: According to [MS-NLMP] Section 3.3.2 only the username and not the
  // domain is uppercased.
  base::string16 upper_username;
  bool result = ToUpper(username, &upper_username);
  DCHECK(result);

  uint8_t v1_hash[kNtlmHashLen];
  GenerateNtlmHashV1(password, v1_hash);
  NtlmBufferWriter input_writer((upper_username.length() + domain.length()) *
                                2);
  bool writer_result = input_writer.WriteUtf16String(upper_username) &&
                       input_writer.WriteUtf16String(domain) &&
                       input_writer.IsEndOfBuffer();
  DCHECK(writer_result);

  bssl::ScopedHMAC_CTX ctx;
  HMAC_Init_ex(ctx.get(), v1_hash, sizeof(v1_hash), EVP_md5(), NULL);
  DCHECK_EQ(sizeof(v1_hash), HMAC_size(ctx.get()));
  HMAC_Update(ctx.get(), input_writer.GetBuffer().data(),
              input_writer.GetLength());
  HMAC_Final(ctx.get(), v2_hash, nullptr);
}

Buffer GenerateProofInputV2(uint64_t timestamp,
                            const uint8_t* client_challenge) {
  NtlmBufferWriter writer(kProofInputLenV2);
  bool result = writer.WriteUInt16(kProofInputVersionV2) &&
                writer.WriteZeros(6) && writer.WriteUInt64(timestamp) &&
                writer.WriteBytes(client_challenge, kChallengeLen) &&
                writer.WriteZeros(4) && writer.IsEndOfBuffer();

  DCHECK(result);
  return writer.Pass();
}

void GenerateNtlmProofV2(const uint8_t* v2_hash,
                         const uint8_t* server_challenge,
                         const Buffer& v2_input,
                         const Buffer& target_info,
                         uint8_t* v2_proof) {
  DCHECK_EQ(kProofInputLenV2, v2_input.size());

  bssl::ScopedHMAC_CTX ctx;
  HMAC_Init_ex(ctx.get(), v2_hash, kNtlmHashLen, EVP_md5(), NULL);
  DCHECK_EQ(kNtlmProofLenV2, HMAC_size(ctx.get()));
  HMAC_Update(ctx.get(), server_challenge, kChallengeLen);
  HMAC_Update(ctx.get(), v2_input.data(), v2_input.size());
  HMAC_Update(ctx.get(), target_info.data(), target_info.size());
  const uint32_t zero = 0;
  HMAC_Update(ctx.get(), reinterpret_cast<const uint8_t*>(&zero),
              sizeof(uint32_t));
  HMAC_Final(ctx.get(), v2_proof, nullptr);
}

void GenerateSessionBaseKeyV2(const uint8_t* v2_hash,
                              const uint8_t* v2_proof,
                              uint8_t* session_key) {
  bssl::ScopedHMAC_CTX ctx;
  HMAC_Init_ex(ctx.get(), v2_hash, kNtlmHashLen, EVP_md5(), NULL);
  DCHECK_EQ(kSessionKeyLenV2, HMAC_size(ctx.get()));
  HMAC_Update(ctx.get(), v2_proof, kNtlmProofLenV2);
  HMAC_Final(ctx.get(), session_key, nullptr);
}

void GenerateChannelBindingHashV2(const std::string& channel_bindings,
                                  base::MD5Digest* channel_bindings_hash) {
  NtlmBufferWriter writer(kEpaUnhashedStructHeaderLen);
  bool result = writer.WriteZeros(16) &&
                writer.WriteUInt32(channel_bindings.length()) &&
                writer.IsEndOfBuffer();
  DCHECK(result);

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, base::StringPiece(reinterpret_cast<const char*>(
                                              writer.GetBuffer().data()),
                                          writer.GetBuffer().size()));
  base::MD5Update(&ctx, channel_bindings);
  base::MD5Final(channel_bindings_hash, &ctx);
}

void GenerateMicV2(const uint8_t* session_key,
                   const Buffer& negotiate_msg,
                   const Buffer& challenge_msg,
                   const Buffer& authenticate_msg,
                   uint8_t* mic) {
  bssl::ScopedHMAC_CTX ctx;
  HMAC_Init_ex(ctx.get(), session_key, kNtlmHashLen, EVP_md5(), NULL);
  DCHECK_EQ(kMicLenV2, HMAC_size(ctx.get()));
  HMAC_Update(ctx.get(), negotiate_msg.data(), negotiate_msg.size());
  HMAC_Update(ctx.get(), challenge_msg.data(), challenge_msg.size());
  HMAC_Update(ctx.get(), authenticate_msg.data(), authenticate_msg.size());
  HMAC_Final(ctx.get(), mic, nullptr);
}

NET_EXPORT_PRIVATE Buffer
GenerateUpdatedTargetInfo(bool is_mic_enabled,
                          bool is_epa_enabled,
                          const std::string& channel_bindings,
                          const std::string& spn,
                          const std::vector<AvPair>& av_pairs,
                          uint64_t* server_timestamp) {
  size_t updated_target_info_len = 0;
  std::vector<AvPair> updated_av_pairs(av_pairs);
  UpdateTargetInfoAvPairs(is_mic_enabled, is_epa_enabled, channel_bindings, spn,
                          &updated_av_pairs, server_timestamp,
                          &updated_target_info_len);
  return WriteUpdatedTargetInfo(updated_av_pairs, updated_target_info_len);
}

}  // namespace ntlm
}  // namespace net
