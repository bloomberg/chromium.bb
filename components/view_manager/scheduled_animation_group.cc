// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/scheduled_animation_group.h"

#include <set>

#include "components/view_manager/server_view.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/transform/transform_type_converters.h"

using mojo::ANIMATION_PROPERTY_NONE;
using mojo::ANIMATION_PROPERTY_OPACITY;
using mojo::ANIMATION_PROPERTY_TRANSFORM;
using mojo::AnimationProperty;

namespace view_manager {
namespace {

using Sequences = std::vector<ScheduledAnimationSequence>;

// Gets the value of |property| from |view| into |value|.
void GetValueFromView(const ServerView* view,
                      AnimationProperty property,
                      ScheduledAnimationValue* value) {
  switch (property) {
    case ANIMATION_PROPERTY_NONE:
      NOTREACHED();
      break;
    case ANIMATION_PROPERTY_OPACITY:
      value->float_value = view->opacity();
      break;
    case ANIMATION_PROPERTY_TRANSFORM:
      value->transform = view->transform();
      break;
  }
}

// Sets the value of |property| from |value| into |view|.
void SetViewPropertyFromValue(ServerView* view,
                              AnimationProperty property,
                              const ScheduledAnimationValue& value) {
  switch (property) {
    case ANIMATION_PROPERTY_NONE:
      break;
    case ANIMATION_PROPERTY_OPACITY:
      view->SetOpacity(value.float_value);
      break;
    case ANIMATION_PROPERTY_TRANSFORM:
      view->SetTransform(value.transform);
      break;
  }
}

// Sets the value of |property| into |view| between two points.
void SetViewPropertyFromValueBetween(ServerView* view,
                                     AnimationProperty property,
                                     double value,
                                     gfx::Tween::Type tween_type,
                                     const ScheduledAnimationValue& start,
                                     const ScheduledAnimationValue& target) {
  const double tween_value = gfx::Tween::CalculateValue(tween_type, value);
  switch (property) {
    case ANIMATION_PROPERTY_NONE:
      break;
    case ANIMATION_PROPERTY_OPACITY:
      view->SetOpacity(gfx::Tween::FloatValueBetween(
          tween_value, start.float_value, target.float_value));
      break;
    case ANIMATION_PROPERTY_TRANSFORM:
      view->SetTransform(gfx::Tween::TransformValueBetween(
          tween_value, start.transform, target.transform));
      break;
  }
}

gfx::Tween::Type AnimationTypeToTweenType(mojo::AnimationTweenType type) {
  switch (type) {
    case mojo::ANIMATION_TWEEN_TYPE_LINEAR:
      return gfx::Tween::LINEAR;
    case mojo::ANIMATION_TWEEN_TYPE_EASE_IN:
      return gfx::Tween::EASE_IN;
    case mojo::ANIMATION_TWEEN_TYPE_EASE_OUT:
      return gfx::Tween::EASE_OUT;
    case mojo::ANIMATION_TWEEN_TYPE_EASE_IN_OUT:
      return gfx::Tween::EASE_IN_OUT;
  }
  return gfx::Tween::LINEAR;
}

void ConvertToScheduledValue(const mojo::AnimationValue& transport_value,
                             ScheduledAnimationValue* value) {
  value->float_value = transport_value.float_value;
  value->transform = transport_value.transform.To<gfx::Transform>();
}

void ConvertToScheduledElement(const mojo::AnimationElement& transport_element,
                               ScheduledAnimationElement* element) {
  element->property = transport_element.property;
  element->duration =
      base::TimeDelta::FromMicroseconds(transport_element.duration);
  element->tween_type = AnimationTypeToTweenType(transport_element.tween_type);
  if (transport_element.property != ANIMATION_PROPERTY_NONE) {
    if (transport_element.start_value.get()) {
      element->is_start_valid = true;
      ConvertToScheduledValue(*transport_element.start_value,
                              &(element->start_value));
    } else {
      element->is_start_valid = false;
    }
    ConvertToScheduledValue(*transport_element.target_value,
                            &(element->target_value));
  }
}

bool IsAnimationValueValid(AnimationProperty property,
                           const mojo::AnimationValue& value) {
  switch (property) {
    case ANIMATION_PROPERTY_NONE:
      NOTREACHED();
      return false;
    case ANIMATION_PROPERTY_OPACITY:
      return value.float_value >= 0.f && value.float_value <= 1.f;
    case ANIMATION_PROPERTY_TRANSFORM:
      return value.transform.get() && value.transform->matrix.size() == 16u;
  }
  return false;
}

bool IsAnimationElementValid(const mojo::AnimationElement& element) {
  if (element.property == ANIMATION_PROPERTY_NONE)
    return true;  // None is a pause and doesn't need any values.
  if (element.start_value.get() &&
      !IsAnimationValueValid(element.property, *element.start_value))
    return false;
  // For all other properties we require a target.
  return element.target_value.get() &&
         IsAnimationValueValid(element.property, *element.target_value);
}

bool IsAnimationSequenceValid(const mojo::AnimationSequence& sequence) {
  if (sequence.elements.size() == 0u)
    return false;

  for (size_t i = 0; i < sequence.elements.size(); ++i) {
    if (!IsAnimationElementValid(*sequence.elements[i]))
      return false;
  }
  return true;
}

bool IsAnimationGroupValid(const mojo::AnimationGroup& transport_group) {
  if (transport_group.sequences.size() == 0u)
    return false;
  for (size_t i = 0; i < transport_group.sequences.size(); ++i) {
    if (!IsAnimationSequenceValid(*transport_group.sequences[i]))
      return false;
  }
  return true;
}

// If the start value for |element| isn't valid, the value for the property
// is obtained from |view| and placed into |element|.
void GetStartValueFromViewIfNecessary(const ServerView* view,
                                      ScheduledAnimationElement* element) {
  if (element->property != ANIMATION_PROPERTY_NONE &&
      !element->is_start_valid) {
    GetValueFromView(view, element->property, &(element->start_value));
  }
}

void GetScheduledAnimationProperties(const Sequences& sequences,
                                     std::set<AnimationProperty>* properties) {
  for (const ScheduledAnimationSequence& sequence : sequences) {
    for (const ScheduledAnimationElement& element : sequence.elements)
      properties->insert(element.property);
  }
}

void SetPropertyToTargetProperty(ServerView* view,
                                 mojo::AnimationProperty property,
                                 const Sequences& sequences) {
  // NOTE: this doesn't deal with |cycle_count| quite right, but I'm honestly
  // not sure we really want to support the same property in multiple sequences
  // animating at once so I'm not dealing.
  base::TimeDelta max_end_duration;
  scoped_ptr<ScheduledAnimationValue> value;
  for (const ScheduledAnimationSequence& sequence : sequences) {
    base::TimeDelta duration;
    for (const ScheduledAnimationElement& element : sequence.elements) {
      if (element.property != property)
        continue;

      duration += element.duration;
      if (duration > max_end_duration) {
        max_end_duration = duration;
        value.reset(new ScheduledAnimationValue(element.target_value));
      }
    }
  }
  if (value.get())
    SetViewPropertyFromValue(view, property, *value);
}

void ConvertSequenceToScheduled(
    const mojo::AnimationSequence& transport_sequence,
    base::TimeTicks now,
    ScheduledAnimationSequence* sequence) {
  sequence->run_until_stopped = transport_sequence.cycle_count == 0u;
  sequence->cycle_count = transport_sequence.cycle_count;
  DCHECK_NE(0u, transport_sequence.elements.size());
  sequence->elements.resize(transport_sequence.elements.size());

  base::TimeTicks element_start_time = now;
  for (size_t i = 0; i < transport_sequence.elements.size(); ++i) {
    ConvertToScheduledElement(*(transport_sequence.elements[i].get()),
                              &(sequence->elements[i]));
    sequence->elements[i].start_time = element_start_time;
    sequence->duration += sequence->elements[i].duration;
    element_start_time += sequence->elements[i].duration;
  }
}

bool AdvanceSequence(ServerView* view,
                     ScheduledAnimationSequence* sequence,
                     base::TimeTicks now) {
  ScheduledAnimationElement* element =
      &(sequence->elements[sequence->current_index]);
  while (element->start_time + element->duration < now) {
    SetViewPropertyFromValue(view, element->property, element->target_value);
    if (++sequence->current_index == sequence->elements.size()) {
      if (!sequence->run_until_stopped && --sequence->cycle_count == 0) {
        SetViewPropertyFromValue(view, element->property,
                                 element->target_value);
        return false;
      }

      sequence->current_index = 0;
    }
    sequence->elements[sequence->current_index].start_time =
        element->start_time + element->duration;
    element = &(sequence->elements[sequence->current_index]);
    GetStartValueFromViewIfNecessary(view, element);

    // It's possible for the delta between now and |last_tick_time_| to be very
    // big (could happen if machine sleeps and is woken up much later). Normally
    // the repeat count is smallish, so we don't bother optimizing it. OTOH if
    // a sequence repeats forever we optimize it lest we get stuck in this loop
    // for a very long time.
    if (sequence->run_until_stopped && sequence->current_index == 0) {
      element->start_time =
          now - base::TimeDelta::FromMicroseconds(
                    (now - element->start_time).InMicroseconds() %
                    sequence->duration.InMicroseconds());
    }
  }
  return true;
}

}  // namespace

ScheduledAnimationValue::ScheduledAnimationValue() {
}
ScheduledAnimationValue::~ScheduledAnimationValue() {
}

ScheduledAnimationElement::ScheduledAnimationElement()
    : property(ANIMATION_PROPERTY_OPACITY),
      tween_type(gfx::Tween::EASE_IN),
      is_start_valid(false) {
}
ScheduledAnimationElement::~ScheduledAnimationElement() {
}

ScheduledAnimationSequence::ScheduledAnimationSequence()
    : run_until_stopped(false), cycle_count(0), current_index(0u) {
}
ScheduledAnimationSequence::~ScheduledAnimationSequence() {
}

ScheduledAnimationGroup::~ScheduledAnimationGroup() {
}

// static
scoped_ptr<ScheduledAnimationGroup> ScheduledAnimationGroup::Create(
    ServerView* view,
    base::TimeTicks now,
    uint32_t id,
    const mojo::AnimationGroup& transport_group) {
  if (!IsAnimationGroupValid(transport_group))
    return nullptr;

  scoped_ptr<ScheduledAnimationGroup> group(
      new ScheduledAnimationGroup(view, id, now));
  group->sequences_.resize(transport_group.sequences.size());
  for (size_t i = 0; i < transport_group.sequences.size(); ++i) {
    const mojo::AnimationSequence& transport_sequence(
        *(transport_group.sequences[i]));
    DCHECK_NE(0u, transport_sequence.elements.size());
    ConvertSequenceToScheduled(transport_sequence, now, &group->sequences_[i]);
  }
  return group.Pass();
}

void ScheduledAnimationGroup::ObtainStartValues() {
  for (ScheduledAnimationSequence& sequence : sequences_)
    GetStartValueFromViewIfNecessary(view_, &(sequence.elements[0]));
}

void ScheduledAnimationGroup::SetValuesToTargetValuesForPropertiesNotIn(
    const ScheduledAnimationGroup& other) {
  std::set<AnimationProperty> our_properties;
  GetScheduledAnimationProperties(sequences_, &our_properties);

  std::set<AnimationProperty> other_properties;
  GetScheduledAnimationProperties(other.sequences_, &other_properties);

  for (AnimationProperty property : our_properties) {
    if (other_properties.count(property) == 0 &&
        property != ANIMATION_PROPERTY_NONE) {
      SetPropertyToTargetProperty(view_, property, sequences_);
    }
  }
}

bool ScheduledAnimationGroup::Tick(base::TimeTicks time) {
  for (Sequences::iterator i = sequences_.begin(); i != sequences_.end();) {
    if (!AdvanceSequence(view_, &(*i), time)) {
      i = sequences_.erase(i);
      continue;
    }
    const ScheduledAnimationElement& active_element(
        i->elements[i->current_index]);
    const double percent =
        (time - active_element.start_time).InMillisecondsF() /
        active_element.duration.InMillisecondsF();
    SetViewPropertyFromValueBetween(
        view_, active_element.property, percent, active_element.tween_type,
        active_element.start_value, active_element.target_value);
    ++i;
  }
  return sequences_.empty();
}

ScheduledAnimationGroup::ScheduledAnimationGroup(ServerView* view,
                                                 uint32_t id,
                                                 base::TimeTicks time_scheduled)
    : view_(view), id_(id), time_scheduled_(time_scheduled) {
}

}  // namespace view_manager
