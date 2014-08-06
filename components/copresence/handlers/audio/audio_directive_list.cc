// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_list.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "media/base/audio_bus.h"

namespace {

// UrlSafe is defined as:
// '/' represented by a '_' and '+' represented by a '-'
// TODO(rkc): Move this processing to the whispernet wrapper.
std::string FromUrlSafe(std::string token) {
  base::ReplaceChars(token, "-", "+", &token);
  base::ReplaceChars(token, "_", "/", &token);
  return token;
}

const int kSampleExpiryTimeMs = 60 * 60 * 1000;  // 60 minutes.
const int kMaxSamples = 10000;

}  // namespace

namespace copresence {

// Public methods.

AudioDirective::AudioDirective() {
}

AudioDirective::AudioDirective(const std::string& token,
                               const std::string& op_id,
                               base::Time end_time)
    : token(token), op_id(op_id), end_time(end_time) {
}

AudioDirective::AudioDirective(
    const std::string& token,
    const std::string& op_id,
    base::Time end_time,
    const scoped_refptr<media::AudioBusRefCounted>& samples)
    : token(token), op_id(op_id), end_time(end_time), samples(samples) {
}

AudioDirective::~AudioDirective() {
}

AudioDirectiveList::AudioDirectiveList(
    const EncodeTokenCallback& encode_token_callback,
    const base::Closure& token_added_callback)
    : encode_token_callback_(encode_token_callback),
      token_added_callback_(token_added_callback),
      samples_cache_(base::TimeDelta::FromMilliseconds(kSampleExpiryTimeMs),
                     kMaxSamples) {
}

AudioDirectiveList::~AudioDirectiveList() {
}

void AudioDirectiveList::AddTransmitDirective(const std::string& token,
                                              const std::string& op_id,
                                              base::TimeDelta ttl) {
  std::string valid_token = FromUrlSafe(token);
  base::Time end_time = base::Time::Now() + ttl;

  if (samples_cache_.HasKey(valid_token)) {
    active_transmit_tokens_.push(AudioDirective(
        valid_token, op_id, end_time, samples_cache_.GetValue(valid_token)));
    return;
  }

  // If an encode request for this token has been sent, don't send it again.
  if (pending_transmit_tokens_.find(valid_token) !=
      pending_transmit_tokens_.end()) {
    return;
  }

  pending_transmit_tokens_[valid_token] =
      AudioDirective(valid_token, op_id, end_time);
  // All whispernet callbacks will be cleared before we are destructed, so
  // unretained is safe to use here.
  encode_token_callback_.Run(
      valid_token,
      base::Bind(&AudioDirectiveList::OnTokenEncoded, base::Unretained(this)));
}

void AudioDirectiveList::AddReceiveDirective(const std::string& op_id,
                                             base::TimeDelta ttl) {
  active_receive_tokens_.push(
      AudioDirective(std::string(), op_id, base::Time::Now() + ttl));
}

scoped_ptr<AudioDirective> AudioDirectiveList::GetNextTransmit() {
  return GetNextFromList(&active_transmit_tokens_);
}

scoped_ptr<AudioDirective> AudioDirectiveList::GetNextReceive() {
  return GetNextFromList(&active_receive_tokens_);
}

scoped_ptr<AudioDirective> AudioDirectiveList::GetNextFromList(
    AudioDirectiveQueue* list) {
  CHECK(list);

  // Checks if we have any valid tokens at all (since the top of the list is
  // always pointing to the token with the latest expiry time). If we don't
  // have any valid tokens left, clear the list.
  if (!list->empty() && list->top().end_time < base::Time::Now()) {
    while (!list->empty())
      list->pop();
  }

  if (list->empty())
    return make_scoped_ptr<AudioDirective>(NULL);

  return make_scoped_ptr(new AudioDirective(list->top()));
}

void AudioDirectiveList::OnTokenEncoded(
    const std::string& token,
    const scoped_refptr<media::AudioBusRefCounted>& samples) {
  // We shouldn't re-encode a token if it's already in the cache.
  DCHECK(!samples_cache_.HasKey(token));
  DVLOG(3) << "Token: " << token << " encoded.";
  samples_cache_.Add(token, samples);

  // Copy the samples into their corresponding directive object and move
  // that object into the active queue.
  std::map<std::string, AudioDirective>::iterator it =
      pending_transmit_tokens_.find(token);

  it->second.samples = samples;
  active_transmit_tokens_.push(it->second);
  pending_transmit_tokens_.erase(it);

  if (!token_added_callback_.is_null())
    token_added_callback_.Run();
}

}  // namespace copresence
