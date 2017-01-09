// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/actions_parser.h"

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace content {

namespace {

SyntheticPointerActionParams::PointerActionType ToSyntheticPointerActionType(
    std::string action_type) {
  if (action_type == "pointerDown")
    return SyntheticPointerActionParams::PointerActionType::PRESS;
  if (action_type == "pointerMove")
    return SyntheticPointerActionParams::PointerActionType::MOVE;
  if (action_type == "pointerUp")
    return SyntheticPointerActionParams::PointerActionType::RELEASE;
  if (action_type == "pause")
    return SyntheticPointerActionParams::PointerActionType::IDLE;
  return SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED;
}

SyntheticGestureParams::GestureSourceType ToSyntheticGestureSourceType(
    std::string source_type) {
  if (source_type == "touch")
    return SyntheticGestureParams::TOUCH_INPUT;
  else if (source_type == "mouse")
    return SyntheticGestureParams::MOUSE_INPUT;
  else
    return SyntheticGestureParams::DEFAULT_INPUT;
}

}  // namespace

ActionsParser::ActionsParser(base::Value* pointer_actions_value)
    : longest_action_sequence_(0),
      pointer_actions_value_(pointer_actions_value),
      action_index_(0) {}

ActionsParser::~ActionsParser() {}

bool ActionsParser::ParsePointerActionSequence() {
  const base::ListValue* pointer_list;
  if (!pointer_actions_value_->GetAsList(&pointer_list)) {
    error_message_ =
        base::StringPrintf("pointer_list is missing or not a list");
    return false;
  }

  for (const auto& pointer_value : *pointer_list) {
    const base::DictionaryValue* pointer_actions;
    if (!pointer_value->GetAsDictionary(&pointer_actions)) {
      error_message_ =
          base::StringPrintf("pointer actions is missing or not a dictionary");
      return false;
    } else if (!ParsePointerActions(*pointer_actions)) {
      return false;
    }
    action_index_++;
  }

  if (!gesture_params_)
    gesture_params_ = base::MakeUnique<SyntheticPointerActionListParams>();

  gesture_params_->gesture_source_type =
      ToSyntheticGestureSourceType(source_type_);
  // Group a list of actions from all pointers into a
  // SyntheticPointerActionListParams object, which is a list of actions, which
  // will be dispatched together.
  for (size_t action_index = 0; action_index < longest_action_sequence_;
       ++action_index) {
    SyntheticPointerActionListParams::ParamList param_list;
    for (const auto pointer_list : pointer_actions_list_) {
      if (action_index < pointer_list.size())
        param_list.push_back(pointer_list[action_index]);
    }
    gesture_params_->PushPointerActionParamsList(param_list);
  }

  return true;
}

bool ActionsParser::ParsePointerActions(const base::DictionaryValue& pointer) {
  std::string source_type;
  if (!pointer.GetString("source", &source_type)) {
    error_message_ =
        base::StringPrintf("source type is missing or not a string");
    return false;
  } else if (source_type != "touch" && source_type != "mouse" &&
             source_type != "pointer") {
    error_message_ =
        base::StringPrintf("source type is an unsupported input source");
    return false;
  }

  if (source_type_.empty()) {
    source_type_ = source_type;

#if defined(OS_MACOSX)
    if (source_type == "touch") {
      error_message_ =
          base::StringPrintf("Mac OS does not support touch events");
      return false;
    }
#endif  // defined(OS_MACOSX)
  }

  if (source_type_ != source_type) {
    error_message_ = base::StringPrintf(
        "currently multiple input sources are not not supported");
    return false;
  }

  if (source_type != "touch" && action_index_ > 0) {
    error_message_ = base::StringPrintf(
        "for input source type of mouse and pen, we only support one device in "
        "one sequence");
    return false;
  }

  const base::ListValue* actions;
  if (!pointer.GetList("actions", &actions)) {
    error_message_ = base::StringPrintf(
        "pointer[%d].actions is missing or not a list", action_index_);
    return false;
  }

  if (actions->GetSize() > longest_action_sequence_)
    longest_action_sequence_ = actions->GetSize();

  if (!ParseActions(*actions))
    return false;

  return true;
}

bool ActionsParser::ParseActions(const base::ListValue& actions) {
  SyntheticPointerActionListParams::ParamList param_list;
  for (const auto& action_value : actions) {
    const base::DictionaryValue* action;
    if (!action_value->GetAsDictionary(&action)) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions is missing or not a dictionary", action_index_);
      return false;
    } else if (!ParseAction(*action, param_list)) {
      return false;
    }
  }
  pointer_actions_list_.push_back(param_list);
  return true;
}

bool ActionsParser::ParseAction(
    const base::DictionaryValue& action,
    SyntheticPointerActionListParams::ParamList& param_list) {
  SyntheticPointerActionParams::PointerActionType pointer_action_type =
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED;
  std::string name;
  if (!action.GetString("name", &name)) {
    error_message_ = base::StringPrintf(
        "actions[%d].actions.name is missing or not a string", action_index_);
    return false;
  }
  pointer_action_type = ToSyntheticPointerActionType(name);
  if (pointer_action_type ==
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED) {
    error_message_ = base::StringPrintf(
        "actions[%d].actions.name is an unsupported action name",
        action_index_);
    return false;
  }

  double position_x = 0;
  double position_y = 0;
  if (action.HasKey("x") && !action.GetDouble("x", &position_x)) {
    error_message_ = base::StringPrintf("actions[%d].actions.x is not a number",
                                        action_index_);
    return false;
  }

  if (action.HasKey("y") && !action.GetDouble("y", &position_y)) {
    error_message_ = base::StringPrintf("actions[%d].actions.y is not a number",
                                        action_index_);
    return false;
  }

  SyntheticPointerActionParams action_param(pointer_action_type);
  action_param.set_index(action_index_);
  if (pointer_action_type ==
          SyntheticPointerActionParams::PointerActionType::PRESS ||
      pointer_action_type ==
          SyntheticPointerActionParams::PointerActionType::MOVE) {
    action_param.set_position(gfx::PointF(position_x, position_y));
  }
  param_list.push_back(action_param);
  return true;
}

}  // namespace content
