// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_EVENT_PREDICTION_H_
#define CONTENT_RENDERER_INPUT_INPUT_EVENT_PREDICTION_H_

#include <list>
#include <unordered_map>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/input/input_predictor.h"
#include "third_party/blink/public/platform/input/predictor_factory.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/event.h"

using blink::WebInputEvent;
using blink::WebPointerProperties;

namespace content {

// Handle resampling of WebMouseEvent, WebTouchEvent and WebPointerEvent.
// This class stores prediction of all active pointers.
class CONTENT_EXPORT InputEventPrediction {
 public:
  // enable_resampling is true when kResamplingInputEvents is enabled.
  explicit InputEventPrediction(bool enable_resampling);
  ~InputEventPrediction();

  // Handle Resampling/Prediction of WebInputEvents. This function is mainly
  // doing three things:
  // 1. Maintain/Updates predictor using current CoalescedEvents vector.
  // 2. When enable_resampling is true, change coalesced_event->EventPointer()'s
  //    coordinates to the position at frame time.
  // 3. Generates 3 predicted events when prediction is available, add the
  //    PredictedEvent to coalesced_event.
  void HandleEvents(blink::WebCoalescedInputEvent& coalesced_event,
                    base::TimeTicks frame_time);

  // Initialize predictor for different pointer.
  std::unique_ptr<blink::InputPredictor> CreatePredictor() const;

 private:
  friend class InputEventPredictionTest;
  FRIEND_TEST_ALL_PREFIXES(InputEventPredictionTest, PredictorType);
  FRIEND_TEST_ALL_PREFIXES(InputEventPredictionTest, ResamplingDisabled);
  FRIEND_TEST_ALL_PREFIXES(InputEventPredictionTest,
                           NoResampleWhenExceedMaxResampleTime);

  // The following functions are for handling multiple TouchPoints in a
  // WebTouchEvent. They should be more neat when WebTouchEvent is elimated.
  // Cast events from WebInputEvent to WebPointerProperties. Call
  // UpdateSinglePointer for each pointer.
  void UpdatePrediction(const WebInputEvent& event);
  // Cast events from WebInputEvent to WebPointerProperties. Call
  // ResamplingSinglePointer for each pointer.
  void ApplyResampling(base::TimeTicks frame_time, WebInputEvent* event);
  // Reset predictor for each pointer in WebInputEvent by  ResetSinglePredictor.
  void ResetPredictor(const WebInputEvent& event);

  // Add predicted events to WebCoalescedInputEvent if prediction is available.
  void AddPredictedEvents(blink::WebCoalescedInputEvent& coalesced_event);

  // Get time interval of a pointer. Default to mouse predictor if there is no
  // predictor for pointer.
  base::TimeDelta GetPredictionTimeInterval(
      const WebPointerProperties& event) const;

  // Returns a pointer to the predictor for given WebPointerProperties.
  blink::InputPredictor* GetPredictor(const WebPointerProperties& event) const;

  // Get single predictor based on event id and type, and update the predictor
  // with new events coords.
  void UpdateSinglePointer(const WebPointerProperties& event,
                           base::TimeTicks time);

  // Get prediction result of a single predictor based on the predict_time,
  // and apply predicted result to the event. Return false if no prediction
  // available.
  bool GetPointerPrediction(base::TimeTicks predict_time,
                            WebPointerProperties* event);

  // Get single predictor based on event id and type. For mouse, reset the
  // predictor, for other pointer type, remove it from mapping.
  void ResetSinglePredictor(const WebPointerProperties& event);

  std::unordered_map<ui::PointerId, std::unique_ptr<blink::InputPredictor>>
      pointer_id_predictor_map_;
  std::unique_ptr<blink::InputPredictor> mouse_predictor_;

  // Store the field trial parameter used for choosing different types of
  // predictor.
  blink::input_prediction::PredictorType selected_predictor_type_;

  bool enable_resampling_ = false;

  // Records the timestamp for last event added to predictor.
  base::TimeTicks last_event_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(InputEventPrediction);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_EVENT_PREDICTION_H_
