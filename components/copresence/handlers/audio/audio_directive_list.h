// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_LIST_
#define COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_LIST_

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/copresence/timed_map.h"

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

struct AudioDirective {
  // Default ctor, required by the priority queue.
  AudioDirective();
  // ctor used to store transmit directives that are awaiting samples.
  AudioDirective(const std::string& token,
                 const std::string& op_id,
                 base::Time end_time);
  // ctor used to construct a complete transmit directive.
  AudioDirective(const std::string& token,
                 const std::string& op_id,
                 base::Time end_time,
                 const scoped_refptr<media::AudioBusRefCounted>& samples);
  ~AudioDirective();

  std::string token;
  std::string op_id;
  base::Time end_time;
  scoped_refptr<media::AudioBusRefCounted> samples;
};

// This class maintains a list of active audio directives. It fetches the audio
// samples associated with a audio transmit directives and expires directives
// that have outlived their TTL.
// TODO(rkc): Once we implement more token technologies, move reusable code
// from here to a base class and inherit various XxxxDirectiveList
// classes from it.
class AudioDirectiveList {
 public:
  typedef base::Callback<void(const std::string&,
                              bool,
                              const scoped_refptr<media::AudioBusRefCounted>&)>
      SamplesCallback;
  typedef base::Callback<void(const std::string&, bool, const SamplesCallback&)>
      EncodeTokenCallback;

  AudioDirectiveList(const EncodeTokenCallback& encode_token_callback,
                     const base::Closure& token_added_callback,
                     bool use_audible_encoding);
  virtual ~AudioDirectiveList();

  // Adds a token to the token queue, after getting its corresponding samples
  // from whispernet.
  void AddTransmitDirective(const std::string& token,
                            const std::string& op_id,
                            base::TimeDelta ttl);

  void AddReceiveDirective(const std::string& op_id, base::TimeDelta ttl);

  // Returns the next audio token to play. This also cleans up expired tokens.
  scoped_ptr<AudioDirective> GetNextTransmit();
  scoped_ptr<AudioDirective> GetNextReceive();

  // This is the method that the whispernet client needs to call to return
  // samples to us.
  void OnTokenEncoded(const std::string& token,
                      bool audible,
                      const scoped_refptr<media::AudioBusRefCounted>& samples);

 private:
  // Comparator for comparing end_times on audio tokens.
  class LatestFirstComparator {
   public:
    // This will sort our queue with the 'latest' time being the top.
    bool operator()(const AudioDirective& left,
                    const AudioDirective& right) const {
      return left.end_time < right.end_time;
    }
  };

  typedef std::priority_queue<AudioDirective,
                              std::vector<AudioDirective>,
                              LatestFirstComparator> AudioDirectiveQueue;
  typedef TimedMap<std::string, scoped_refptr<media::AudioBusRefCounted> >
      SamplesMap;

  scoped_ptr<AudioDirective> GetNextFromList(AudioDirectiveQueue* list);

  // A map of tokens that are awaiting their samples before we can
  // add them to the active transmit tokens list.
  std::map<std::string, AudioDirective> pending_transmit_tokens_;

  AudioDirectiveQueue active_transmit_tokens_;
  AudioDirectiveQueue active_receive_tokens_;

  EncodeTokenCallback encode_token_callback_;
  base::Closure token_added_callback_;
  const bool use_audible_encoding_;

  // Cache that holds the encoded samples. After reaching its limit, the cache
  // expires the oldest samples first.
  SamplesMap samples_cache_;

  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveList);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_LIST_
