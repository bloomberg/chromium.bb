// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_MODEL_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/configuration.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement_tracker {

// A Model provides all necessary runtime state.
class Model {
 public:
  // Callback for when model initialization has finished. The bool argument
  // denotes whether the model was successfully initialized.
  using OnModelInitializationFinished = base::Callback<void(bool)>;

  virtual ~Model() = default;

  // Initialize the model, including all underlying sub systems. When all
  // required operations have been finished, a callback is posted.
  virtual void Initialize(const OnModelInitializationFinished& callback) = 0;

  // Returns whether the model is ready, i.e. whether it has been fully
  // initialized.
  virtual bool IsReady() const = 0;

  // Returns the FeatureConfig for the given |feature|.
  virtual const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const = 0;

  // Update the state of whether any in-product help is currently showing.
  virtual void SetIsCurrentlyShowing(bool is_showing) = 0;

  // Returns whether any in-product help is currently showing.
  virtual bool IsCurrentlyShowing() const = 0;

 protected:
  Model() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Model);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_MODEL_H_
