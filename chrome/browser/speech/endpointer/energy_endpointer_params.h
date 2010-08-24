// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_ENDPOINTER_ENERGY_ENDPOINTER_PARAMS_H_
#define CHROME_BROWSER_SPEECH_ENDPOINTER_ENERGY_ENDPOINTER_PARAMS_H_

#include "base/basictypes.h"

namespace speech_input {

// Input parameters for the EnergyEndpointer class.
class EnergyEndpointerParams {
 public:
  EnergyEndpointerParams() {
    SetDefaults();
  }

  void SetDefaults() {
    frame_period_ = 0.01f;
    frame_duration_ = 0.01f;
    endpoint_margin_ = 0.2f;
    onset_window_ = 0.15f;
    speech_on_window_ = 0.4f;
    offset_window_ = 0.15f;
    onset_detect_dur_ = 0.09f;
    onset_confirm_dur_ = 0.075f;
    on_maintain_dur_ = 0.10f;
    offset_confirm_dur_ = 0.12f;
    decision_threshold_ = 150.0f;
    min_decision_threshold_ = 50.0f;
    fast_update_dur_ = 0.2f;
    sample_rate_ = 8000.0f;
    min_fundamental_frequency_ = 57.143f;
    max_fundamental_frequency_ = 400.0f;
    contamination_rejection_period_ = 0.25f;
  }

  void operator=(const EnergyEndpointerParams& source) {
    frame_period_ = source.frame_period();
    frame_duration_ = source.frame_duration();
    endpoint_margin_ = source.endpoint_margin();
    onset_window_ = source.onset_window();
    speech_on_window_ = source.speech_on_window();
    offset_window_ = source.offset_window();
    onset_detect_dur_ = source.onset_detect_dur();
    onset_confirm_dur_ = source.onset_confirm_dur();
    on_maintain_dur_ = source.on_maintain_dur();
    offset_confirm_dur_ = source.offset_confirm_dur();
    decision_threshold_ = source.decision_threshold();
    min_decision_threshold_ = source.min_decision_threshold();
    fast_update_dur_ = source.fast_update_dur();
    sample_rate_ = source.sample_rate();
    min_fundamental_frequency_ = source.min_fundamental_frequency();
    max_fundamental_frequency_ = source.max_fundamental_frequency();
    contamination_rejection_period_ = source.contamination_rejection_period();
  }

  // Accessors and mutators
  float frame_period() const { return frame_period_; }
  void set_frame_period(float frame_period) {
    frame_period_ = frame_period;
  }

  float frame_duration() const { return frame_duration_; }
  void set_frame_duration(float frame_duration) {
    frame_duration_ = frame_duration;
  }

  float endpoint_margin() const { return endpoint_margin_; }
  void set_endpoint_margin(float endpoint_margin) {
    endpoint_margin_ = endpoint_margin;
  }

  float onset_window() const { return onset_window_; }
  void set_onset_window(float onset_window) { onset_window_ = onset_window; }

  float speech_on_window() const { return speech_on_window_; }
  void set_speech_on_window(float speech_on_window) {
    speech_on_window_ = speech_on_window;
  }

  float offset_window() const { return offset_window_; }
  void set_offset_window(float offset_window) {
    offset_window_ = offset_window;
  }

  float onset_detect_dur() const { return onset_detect_dur_; }
  void set_onset_detect_dur(float onset_detect_dur) {
    onset_detect_dur_ = onset_detect_dur;
  }

  float onset_confirm_dur() const { return onset_confirm_dur_; }
  void set_onset_confirm_dur(float onset_confirm_dur) {
    onset_confirm_dur_ = onset_confirm_dur;
  }

  float on_maintain_dur() const { return on_maintain_dur_; }
  void set_on_maintain_dur(float on_maintain_dur) {
    on_maintain_dur_ = on_maintain_dur;
  }

  float offset_confirm_dur() const { return offset_confirm_dur_; }
  void set_offset_confirm_dur(float offset_confirm_dur) {
    offset_confirm_dur_ = offset_confirm_dur;
  }

  float decision_threshold() const { return decision_threshold_; }
  void set_decision_threshold(float decision_threshold) {
    decision_threshold_ = decision_threshold;
  }

  float min_decision_threshold() const { return min_decision_threshold_; }
  void set_min_decision_threshold(float min_decision_threshold) {
    min_decision_threshold_ = min_decision_threshold;
  }

  float fast_update_dur() const { return fast_update_dur_; }
  void set_fast_update_dur(float fast_update_dur) {
    fast_update_dur_ = fast_update_dur;
  }

  float sample_rate() const { return sample_rate_; }
  void set_sample_rate(float sample_rate) { sample_rate_ = sample_rate; }

  float min_fundamental_frequency() const { return min_fundamental_frequency_; }
  void set_min_fundamental_frequency(float min_fundamental_frequency) {
    min_fundamental_frequency_ = min_fundamental_frequency;
  }

  float max_fundamental_frequency() const { return max_fundamental_frequency_; }
  void set_max_fundamental_frequency(float max_fundamental_frequency) {
    max_fundamental_frequency_ = max_fundamental_frequency;
  }

  float contamination_rejection_period() const {
    return contamination_rejection_period_;
  }
  void set_contamination_rejection_period(
      float contamination_rejection_period) {
    contamination_rejection_period_ = contamination_rejection_period;
  }

 private:
  float frame_period_;  // Frame period
  float frame_duration_;  // Window size
  float onset_window_;  // Interval scanned for onset activity
  float speech_on_window_;  // Inverval scanned for ongoing speech
  float offset_window_;  // Interval scanned for offset evidence
  float offset_confirm_dur_;  // Silence duration required to confirm offset
  float decision_threshold_;  // Initial rms detection threshold
  float min_decision_threshold_;  // Minimum rms detection threshold
  float fast_update_dur_;  // Period for initial estimation of levels.
  float sample_rate_;  // Expected sample rate.

  // Time to add on either side of endpoint threshold crossings
  float endpoint_margin_;
  // Total dur within onset_window required to enter ONSET state
  float onset_detect_dur_;
  // Total on time within onset_window required to enter SPEECH_ON state
  float onset_confirm_dur_;
  // Minimum dur in SPEECH_ON state required to maintain ON state
  float on_maintain_dur_;
  // Minimum fundamental frequency for autocorrelation.
  float min_fundamental_frequency_;
  // Maximum fundamental frequency for autocorrelation.
  float max_fundamental_frequency_;
  // Period after start of user input that above threshold values are ignored.
  // This is to reject audio feedback contamination.
  float contamination_rejection_period_;
};

}  //  namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_ENDPOINTER_ENERGY_ENDPOINTER_PARAMS_H_
