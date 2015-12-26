// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_debug_writer.h"

#include <utility>

#include "content/public/browser/browser_thread.h"
#include "media/base/audio_bus.h"

namespace content {

AudioInputDebugWriter::AudioInputDebugWriter(base::File file)
    : file_(std::move(file)), interleaved_data_size_(0), weak_factory_(this) {}

AudioInputDebugWriter::~AudioInputDebugWriter() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
}

void AudioInputDebugWriter::Write(scoped_ptr<media::AudioBus> data) {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&AudioInputDebugWriter::DoWrite,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&data)));
}

void AudioInputDebugWriter::DoWrite(scoped_ptr<media::AudioBus> data) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // Convert to 16 bit audio and write to file.
  int data_size = data->frames() * data->channels();
  if (!interleaved_data_ || interleaved_data_size_ < data_size) {
    interleaved_data_.reset(new int16_t[data_size]);
    interleaved_data_size_ = data_size;
  }
  data->ToInterleaved(data->frames(), sizeof(interleaved_data_[0]),
                      interleaved_data_.get());
  file_.WriteAtCurrentPos(reinterpret_cast<char*>(interleaved_data_.get()),
                          data_size * sizeof(interleaved_data_[0]));
}

}  // namspace content
