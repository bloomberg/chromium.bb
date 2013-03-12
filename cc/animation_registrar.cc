// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation_registrar.h"

#include "cc/layer_animation_controller.h"

namespace cc {

AnimationRegistrar::AnimationRegistrar() { }
AnimationRegistrar::~AnimationRegistrar()
{
    AnimationControllerMap copy = all_animation_controllers_;
    for (AnimationControllerMap::iterator iter = copy.begin(); iter != copy.end(); ++iter)
        (*iter).second->SetAnimationRegistrar(NULL);
}

scoped_refptr<LayerAnimationController>
AnimationRegistrar::GetAnimationControllerForId(int id)
{
    scoped_refptr<LayerAnimationController> toReturn;
    if (!ContainsKey(all_animation_controllers_, id)) {
        toReturn = LayerAnimationController::Create(id);
        toReturn->SetAnimationRegistrar(this);
        all_animation_controllers_[id] = toReturn.get();
    } else
        toReturn = all_animation_controllers_[id];
    return toReturn;
}

void AnimationRegistrar::DidActivateAnimationController(LayerAnimationController* controller) {
    active_animation_controllers_[controller->id()] = controller;
}

void AnimationRegistrar::DidDeactivateAnimationController(LayerAnimationController* controller) {
    if (ContainsKey(active_animation_controllers_, controller->id()))
        active_animation_controllers_.erase(controller->id());
}

void AnimationRegistrar::RegisterAnimationController(LayerAnimationController* controller) {
    all_animation_controllers_[controller->id()] = controller;
}

void AnimationRegistrar::UnregisterAnimationController(LayerAnimationController* controller) {
    if (ContainsKey(all_animation_controllers_, controller->id()))
        all_animation_controllers_.erase(controller->id());
    DidDeactivateAnimationController(controller);
}

}  // namespace cc
