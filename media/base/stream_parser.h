// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_H_
#define MEDIA_BASE_STREAM_PARSER_H_

#include <deque>
#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"

namespace media {

class AudioDecoderConfig;
class StreamParserBuffer;
class TextTrackConfig;
class VideoDecoderConfig;

// Abstract interface for parsing media byte streams.
class MEDIA_EXPORT StreamParser {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;
  typedef std::map<int, TextTrackConfig> TextTrackConfigMap;

  StreamParser();
  virtual ~StreamParser();

  // Indicates completion of parser initialization.
  // First parameter - Indicates initialization success. Set to true if
  //                   initialization was successful. False if an error
  //                   occurred.
  // Second parameter -  Indicates the stream duration. Only contains a valid
  //                     value if the first parameter is true.
  typedef base::Callback<void(bool, base::TimeDelta)> InitCB;

  // Indicates when new stream configurations have been parsed.
  // First parameter - The new audio configuration. If the config is not valid
  //                   then it means that there isn't an audio stream.
  // Second parameter - The new video configuration. If the config is not valid
  //                    then it means that there isn't an audio stream.
  // Third parameter - The new text tracks configuration.  If the map is empty,
  //                   then no text tracks were parsed from the stream.
  // Return value - True if the new configurations are accepted.
  //                False if the new configurations are not supported
  //                and indicates that a parsing error should be signalled.
  typedef base::Callback<bool(const AudioDecoderConfig&,
                              const VideoDecoderConfig&,
                              const TextTrackConfigMap&)> NewConfigCB;

  // New stream buffers have been parsed.
  // First parameter - A queue of newly parsed audio buffers.
  // Second parameter - A queue of newly parsed video buffers.
  // Return value - True indicates that the buffers are accepted.
  //                False if something was wrong with the buffers and a parsing
  //                error should be signalled.
  typedef base::Callback<bool(const BufferQueue&,
                              const BufferQueue&)> NewBuffersCB;

  // New stream buffers of inband text have been parsed.
  // First parameter - The id of the text track to which these cues will
  //                   be added.
  // Second parameter - A queue of newly parsed buffers.
  // Return value - True indicates that the buffers are accepted.
  //                False if something was wrong with the buffers and a parsing
  //                error should be signalled.
  typedef base::Callback<bool(int, const BufferQueue&)> NewTextBuffersCB;

  // Signals the beginning of a new media segment.
  typedef base::Callback<void()> NewMediaSegmentCB;

  // A new potentially encrypted stream has been parsed.
  // First parameter - The type of the initialization data associated with the
  //                   stream.
  // Second parameter - The initialization data associated with the stream.
  typedef base::Callback<void(const std::string&,
                              const std::vector<uint8>&)> NeedKeyCB;

  // Initialize the parser with necessary callbacks. Must be called before any
  // data is passed to Parse(). |init_cb| will be called once enough data has
  // been parsed to determine the initial stream configurations, presentation
  // start time, and duration.
  virtual void Init(const InitCB& init_cb,
                    const NewConfigCB& config_cb,
                    const NewBuffersCB& new_buffers_cb,
                    const NewTextBuffersCB& text_cb,
                    const NeedKeyCB& need_key_cb,
                    const NewMediaSegmentCB& new_segment_cb,
                    const base::Closure& end_of_segment_cb,
                    const LogCB& log_cb) = 0;

  // Called when a seek occurs. This flushes the current parser state
  // and puts the parser in a state where it can receive data for the new seek
  // point.
  virtual void Flush() = 0;

  // Called when there is new data to parse.
  //
  // Returns true if the parse succeeds.
  virtual bool Parse(const uint8* buf, int size) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamParser);
};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_H_
