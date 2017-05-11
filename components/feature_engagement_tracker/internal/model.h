// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_MODEL_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement_tracker {
class Event;
struct FeatureConfig;

// A Model provides all necessary runtime state.
class Model {
 public:
  // Callback for when model initialization has finished. The |success|
  // argument denotes whether the model was successfully initialized.
  using OnModelInitializationFinished = base::Callback<void(bool success)>;

  virtual ~Model() = default;

  // Initialize the model, including all underlying sub systems. When all
  // required operations have been finished, a callback is posted.
  virtual void Initialize(const OnModelInitializationFinished& callback) = 0;

  // Returns whether the model is ready, i.e. whether it has been successfully
  // initialized.
  virtual bool IsReady() const = 0;

  // Returns the FeatureConfig for the given |feature|.
  virtual const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const = 0;

  // Update the state of whether any in-product help is currently showing.
  virtual void SetIsCurrentlyShowing(bool is_showing) = 0;

  // Returns whether any in-product help is currently showing.
  virtual bool IsCurrentlyShowing() const = 0;

  // Retrieves the Event object for the event with the given name. If the event
  // is not found, a nullptr will be returned. Calling this before the
  // Model has finished initializing will result in undefined behavior.
  virtual const Event* GetEvent(const std::string& event_name) const = 0;

  // Increments the counter for today for how many times the event has happened.
  // If the event has never happened before, the Event object will be created.
  // The |current_day| should be the number of days since UNIX epoch (see
  // TimeProvider::GetCurrentDay()).
  virtual void IncrementEvent(const std::string& event_name,
                              uint32_t current_day) = 0;

 protected:
  Model() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Model);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_MODEL_H_
