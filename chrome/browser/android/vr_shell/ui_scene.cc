// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene.h"

#include "base/values.h"
#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/easing.h"
#include "chrome/browser/android/vr_shell/ui_elements.h"

namespace vr_shell {

namespace {

void ParseRecti(const base::DictionaryValue& dict, const std::string& key,
                Recti* output) {
  const base::DictionaryValue* item_dict;
  if (dict.GetDictionary(key, &item_dict)) {
    CHECK(item_dict->GetInteger("x", &output->x));
    CHECK(item_dict->GetInteger("y", &output->y));
    CHECK(item_dict->GetInteger("width", &output->width));
    CHECK(item_dict->GetInteger("height", &output->height));
  }
}

void Parse2DVec3f(const base::DictionaryValue& dict, const std::string& key,
                  gvr::Vec3f* output) {
  const base::DictionaryValue* item_dict;
  if (dict.GetDictionary(key, &item_dict)) {
    double value;
    CHECK(item_dict->GetDouble("x", &value));
    output->x = value;
    CHECK(item_dict->GetDouble("y", &value));
    output->y = value;
    output->z = 1.0f;
  }
}

void ParseVec3f(const base::DictionaryValue& dict, const std::string& key,
                gvr::Vec3f* output) {
  const base::DictionaryValue* item_dict;
  if (dict.GetDictionary(key, &item_dict)) {
    double value;
    CHECK(item_dict->GetDouble("x", &value));
    output->x = value;
    CHECK(item_dict->GetDouble("y", &value));
    output->y = value;
    CHECK(item_dict->GetDouble("z", &value));
    output->z = value;
  }
}

void ParseRotationAxisAngle(const base::DictionaryValue& dict,
                            const std::string& key, RotationAxisAngle* output) {
  const base::DictionaryValue* item_dict;
  if (dict.GetDictionary(key, &item_dict)) {
    double value;
    CHECK(item_dict->GetDouble("x", &value));
    output->x = value;
    CHECK(item_dict->GetDouble("y", &value));
    output->y = value;
    CHECK(item_dict->GetDouble("z", &value));
    output->z = value;
    CHECK(item_dict->GetDouble("a", &value));
    output->angle = value;
  }
}

void ParseFloats(const base::DictionaryValue& dict,
                 const std::vector<std::string>& keys,
                 std::vector<float>* vec) {
  for (const auto& key : keys) {
    double value;
    CHECK(dict.GetDouble(key, &value)) << "parsing tag " << key;
    vec->push_back(value);
  }
}

void ParseEndpointToFloats(Animation::Property property,
                           const base::DictionaryValue& dict,
                           std::vector<float>* vec) {
  switch (property) {
    case Animation::Property::COPYRECT:
      ParseFloats(dict, {"x", "y", "width", "height"}, vec);
      break;
    case Animation::Property::SIZE:
      ParseFloats(dict, {"x", "y"}, vec);
      break;
    case Animation::Property::SCALE:
      ParseFloats(dict, {"x", "y", "z"}, vec);
      break;
    case Animation::Property::ROTATION:
      ParseFloats(dict, {"x", "y", "z", "a"}, vec);
      break;
    case Animation::Property::TRANSLATION:
      ParseFloats(dict, {"x", "y", "z"}, vec);
      break;
  }
}

std::unique_ptr<easing::Easing> ParseEasing(
    const base::DictionaryValue& dict) {
  easing::EasingType easingType;
  CHECK(dict.GetInteger("type", reinterpret_cast<int*>(&easingType)));
  std::unique_ptr<easing::Easing> result;

  switch (easingType) {
    case easing::EasingType::LINEAR: {
      result.reset(new easing::Linear());
      break;
    }
    case easing::EasingType::CUBICBEZIER: {
      double p1x, p1y, p2x, p2y;
      CHECK(dict.GetDouble("p1x", &p1x));
      CHECK(dict.GetDouble("p1y", &p1y));
      CHECK(dict.GetDouble("p2x", &p2x));
      CHECK(dict.GetDouble("p2y", &p2y));
      result.reset(new easing::CubicBezier(p1x, p1y, p2x, p2y));
      break;
    }
    case easing::EasingType::EASEIN: {
      double pow;
      CHECK(dict.GetDouble("pow", &pow));
      result.reset(new easing::EaseIn(pow));
      break;
    }
    case easing::EasingType::EASEOUT: {
      double pow;
      CHECK(dict.GetDouble("pow", &pow));
      result.reset(new easing::EaseOut(pow));
      break;
    }
  }
  return result;
}

void ApplyAnchoring(const ContentRectangle& parent, XAnchoring x_anchoring,
                    YAnchoring y_anchoring, ReversibleTransform* transform) {
  // To anchor a child, use the parent's size to find its edge.
  float x_offset;
  switch (x_anchoring) {
    case XLEFT:
      x_offset = -0.5f * parent.size.x;
      break;
    case XRIGHT:
      x_offset = 0.5f * parent.size.x;
      break;
    case XNONE:
      x_offset = 0.0f;
      break;
  }
  float y_offset;
  switch (y_anchoring) {
    case YTOP:
      y_offset = 0.5f * parent.size.y;
      break;
    case YBOTTOM:
      y_offset = -0.5f * parent.size.y;
      break;
    case YNONE:
      y_offset = 0.0f;
      break;
  }
  transform->Translate(x_offset, y_offset, 0);
}

}  // namespace

void UiScene::AddUiElement(std::unique_ptr<ContentRectangle>& element) {
  CHECK_GE(element->id, 0);
  CHECK_EQ(GetUiElementById(element->id), nullptr);
  if (element->parent_id >= 0) {
    CHECK_NE(GetUiElementById(element->parent_id), nullptr);
  } else {
    CHECK_EQ(element->x_anchoring, XAnchoring::XNONE);
    CHECK_EQ(element->y_anchoring, YAnchoring::YNONE);
  }
  ui_elements_.push_back(std::move(element));
}

void UiScene::AddUiElementFromDict(const base::DictionaryValue& dict) {
  int id;
  CHECK(dict.GetInteger("id", &id));
  CHECK_EQ(GetUiElementById(id), nullptr);

  std::unique_ptr<ContentRectangle> element(new ContentRectangle);
  element->id = id;

  ApplyDictToElement(dict, element.get());
  ui_elements_.push_back(std::move(element));
}

void UiScene::UpdateUiElementFromDict(const base::DictionaryValue& dict) {
  int id;
  CHECK(dict.GetInteger("id", &id));
  ContentRectangle* element = GetUiElementById(id);
  CHECK_NE(element, nullptr);
  ApplyDictToElement(dict, element);
}

void UiScene::RemoveUiElement(int element_id) {
  for (auto it = ui_elements_.begin(); it != ui_elements_.end(); ++it) {
    if ((*it)->id == element_id) {
      if ((*it)->content_quad) {
        content_element_ = nullptr;
      }
      ui_elements_.erase(it);
      return;
    }
  }
}

void UiScene::AddAnimation(int element_id,
                           std::unique_ptr<Animation>& animation) {
  ContentRectangle* element = GetUiElementById(element_id);
  CHECK_NE(element, nullptr);
  for (auto& existing_animation : element->animations) {
    CHECK_NE(existing_animation->id, animation->id);
  }
  element->animations.emplace_back(std::move(animation));
}

void UiScene::AddAnimationFromDict(const base::DictionaryValue& dict,
                                   int64_t time_in_micro) {
  int animation_id;
  int element_id;
  Animation::Property property;
  double start_time_ms;
  double duration_ms;

  const base::DictionaryValue* easing_dict = nullptr;
  const base::DictionaryValue* from_dict = nullptr;
  const base::DictionaryValue* to_dict = nullptr;
  std::vector<float> from;
  std::vector<float> to;

  CHECK(dict.GetInteger("id", &animation_id));
  CHECK(dict.GetInteger("meshId", &element_id));
  CHECK(dict.GetInteger("property", reinterpret_cast<int*>(&property)));
  CHECK(dict.GetDouble("startInMillis", &start_time_ms));
  CHECK(dict.GetDouble("durationMillis", &duration_ms));

  CHECK(dict.GetDictionary("easing", &easing_dict));
  auto easing = ParseEasing(*easing_dict);

  CHECK(dict.GetDictionary("to", &to_dict));
  ParseEndpointToFloats(property, *to_dict, &to);

  // The start location is optional.  If not specified, the animation will
  // start from the element's current location.
  dict.GetDictionary("from", &from_dict);
  if (from_dict != nullptr) {
    ParseEndpointToFloats(property, *from_dict, &from);
  }

  int64_t start = time_in_micro + (long)(start_time_ms * 1000.0);
  int64_t duration = duration_ms * 1000.0;

  ContentRectangle* element = GetUiElementById(element_id);
  CHECK_NE(element, nullptr);
  element->animations.emplace_back(std::unique_ptr<Animation>(
      new Animation(
          animation_id, static_cast<Animation::Property>(property),
          std::move(easing), from, to, start, duration)));
}

void UiScene::RemoveAnimation(int element_id, int animation_id) {
  ContentRectangle* element = GetUiElementById(element_id);
  CHECK_NE(element, nullptr);
  auto& animations = element->animations;
  for (auto it = animations.begin(); it != animations.end(); ++it) {
    const Animation& existing_animation = **it;
    if (existing_animation.id == animation_id) {
      animations.erase(it);
      return;
    }
  }
}

void UiScene::HandleCommands(const base::ListValue* commands,
                             int64_t time_in_micro) {
  for (auto& item : *commands) {
    base::DictionaryValue* dict;
    CHECK(item->GetAsDictionary(&dict));

    Command type;
    base::DictionaryValue* data;
    CHECK(dict->GetInteger("type", reinterpret_cast<int*>(&type)));
    CHECK(dict->GetDictionary("data", &data));

    switch (type) {
      case Command::ADD_ELEMENT:
        AddUiElementFromDict(*data);
        break;
      case Command::UPDATE_ELEMENT:
        UpdateUiElementFromDict(*data);
        break;
      case Command::REMOVE_ELEMENT: {
        int element_id;
        CHECK(data->GetInteger("id", &element_id));
        RemoveUiElement(element_id);
        break;
      }
      case Command::ADD_ANIMATION:
        AddAnimationFromDict(*data, time_in_micro);
        break;
      case Command::REMOVE_ANIMATION: {
        int element_id, animation_id;
        CHECK(data->GetInteger("id", &animation_id));
        CHECK(data->GetInteger("meshId", &element_id));
        RemoveAnimation(element_id, animation_id);
        break;
      }
    }
  }
}

void UiScene::UpdateTransforms(float screen_tilt, int64_t time_in_micro) {
  // Process all animations before calculating object transforms.
  for (auto& element : ui_elements_) {
    element->Animate(time_in_micro);
  }
  for (auto& element : ui_elements_) {
    element->transform.MakeIdentity();
    element->transform.Scale(element->size.x, element->size.y, element->size.z);
    ApplyRecursiveTransforms(*element.get(), &element->transform);
    element->transform.Rotate(1.0f, 0.0f, 0.0f, screen_tilt);
  }
}

ContentRectangle* UiScene::GetUiElementById(int element_id) {
  for (auto& element : ui_elements_) {
    if (element->id == element_id) {
      return element.get();
    }
  }
  return nullptr;
}

ContentRectangle* UiScene::GetContentQuad() {
  return content_element_;
}

const std::vector<std::unique_ptr<ContentRectangle>>&
UiScene::GetUiElements() const {
  return ui_elements_;
}

UiScene::UiScene() = default;

UiScene::~UiScene() = default;

int64_t UiScene::TimeInMicroseconds() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

void UiScene::ApplyRecursiveTransforms(const ContentRectangle& element,
                                       ReversibleTransform* transform) {
  transform->Scale(element.scale.x, element.scale.y, element.scale.z);
  transform->Rotate(element.rotation.x, element.rotation.y,
                    element.rotation.z, element.rotation.angle);
  transform->Translate(element.translation.x, element.translation.y,
                       element.translation.z);

  if (element.parent_id >= 0) {
    const ContentRectangle* parent = GetUiElementById(element.parent_id);
    CHECK(parent != nullptr);
    ApplyAnchoring(*parent, element.x_anchoring, element.y_anchoring,
                   transform);
    ApplyRecursiveTransforms(*parent, transform);
  }
}

void UiScene::ApplyDictToElement(const base::DictionaryValue& dict,
                                 ContentRectangle *element) {
  int parent_id;
  if (dict.GetInteger("parentId", &parent_id)) {
    CHECK_GE(parent_id, 0);
    CHECK_NE(GetUiElementById(parent_id), nullptr);
    element->parent_id = parent_id;
  }

  dict.GetBoolean("visible", &element->visible);
  dict.GetBoolean("hitTestable", &element->hit_testable);
  dict.GetBoolean("lockToFov", &element->lock_to_fov);
  ParseRecti(dict, "copyRect", &element->copy_rect);
  Parse2DVec3f(dict, "size", &element->size);
  ParseVec3f(dict, "scale", &element->scale);
  ParseRotationAxisAngle(dict, "rotation", &element->rotation);
  ParseVec3f(dict, "translation", &element->translation);

  if (dict.GetBoolean("contentQuad", &element->content_quad)) {
    if (element->content_quad) {
      CHECK_EQ(content_element_, nullptr);
      content_element_ = element;
    } else {
      if (content_element_ == element) {
        content_element_ = nullptr;
      }
    }
  }

  if (dict.GetInteger("xAnchoring",
                      reinterpret_cast<int*>(&element->x_anchoring))) {
    CHECK_GE(element->parent_id, 0);
  }
  if (dict.GetInteger("yAnchoring",
                      reinterpret_cast<int*>(&element->y_anchoring))) {
    CHECK_GE(element->parent_id, 0);
  }
}

}  // namespace vr_shell
