// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_LIST_H_
#define COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_LIST_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

struct AudioDirective {
  // Default ctor, required by the priority queue.
  AudioDirective();
  AudioDirective(const std::string& op_id, base::Time end_time);

  std::string op_id;
  base::Time end_time;
};

// This class maintains a list of active audio directives. It fetches the audio
// samples associated with a audio transmit directives and expires directives
// that have outlived their TTL.
// TODO(rkc): Once we implement more token technologies, move reusable code
// from here to a base class and inherit various XxxxDirectiveList
// classes from it.
class AudioDirectiveList {
 public:
  AudioDirectiveList();
  virtual ~AudioDirectiveList();

  void AddDirective(const std::string& op_id, base::TimeDelta ttl);
  void RemoveDirective(const std::string& op_id);

  scoped_ptr<AudioDirective> GetActiveDirective();

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

  std::vector<AudioDirective>::iterator FindDirectiveByOpId(
      const std::string& op_id);

  // This vector will be organized as a heap with the latest time as the first
  // element. Only currently active directives will exist in this list.
  std::vector<AudioDirective> active_directives_;

  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveList);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_LIST_H_
