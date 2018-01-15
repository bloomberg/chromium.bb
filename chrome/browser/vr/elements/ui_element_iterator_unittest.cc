// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element.h"

#include "base/containers/adapters.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/vr/ui_scene.h"

namespace vr {

namespace {

// Constructs a tree of the following form
// 1 kRoot
//   2 k2dBrowsingRoot
//     3 kFloor
//     4 k2dBrowsingContentGroup
//       5 kBackplane
//       6 kContentQuad
//       7 kUrlBar
//     8 kCeiling
void MakeTree(UiScene* scene) {
  auto element = std::make_unique<UiElement>();
  element->SetName(k2dBrowsingRoot);
  scene->AddUiElement(kRoot, std::move(element));

  element = std::make_unique<UiElement>();
  element->SetName(kFloor);
  scene->AddUiElement(k2dBrowsingRoot, std::move(element));

  element = std::make_unique<UiElement>();
  element->SetName(k2dBrowsingContentGroup);
  scene->AddUiElement(k2dBrowsingRoot, std::move(element));

  element = std::make_unique<UiElement>();
  element->SetName(kBackplane);
  scene->AddUiElement(k2dBrowsingContentGroup, std::move(element));

  element = std::make_unique<UiElement>();
  element->SetName(kContentQuad);
  scene->AddUiElement(k2dBrowsingContentGroup, std::move(element));

  element = std::make_unique<UiElement>();
  element->SetName(kUrlBar);
  scene->AddUiElement(k2dBrowsingContentGroup, std::move(element));

  element = std::make_unique<UiElement>();
  element->SetName(kCeiling);
  scene->AddUiElement(k2dBrowsingRoot, std::move(element));
}

template <typename T>
void CollectElements(T* e, std::vector<T*>* elements) {
  elements->push_back(e);
  for (auto& child : e->children()) {
    CollectElements(child.get(), elements);
  }
}

}  // namespace

struct UiElementIteratorTestCase {
  UiElementName root;
  size_t num_elements_in_subtree;
};

class UiElementIteratorTest
    : public ::testing::TestWithParam<UiElementIteratorTestCase> {};

TEST_P(UiElementIteratorTest, VerifyTraversal) {
  UiScene scene;
  MakeTree(&scene);
  std::vector<UiElement*> elements;
  CollectElements(scene.GetUiElementByName(GetParam().root), &elements);

  size_t i = 0;
  for (auto& e : *(scene.GetUiElementByName(GetParam().root))) {
    EXPECT_GT(elements.size(), i);
    EXPECT_EQ(elements[i++]->id(), e.id());
  }
  EXPECT_EQ(elements.size(), i);
  EXPECT_EQ(GetParam().num_elements_in_subtree, i);

  i = 0;
  for (auto& e : *const_cast<const UiElement*>(
           scene.GetUiElementByName(GetParam().root))) {
    EXPECT_GT(elements.size(), i);
    EXPECT_EQ(elements[i++]->id(), e.id());
  }
  EXPECT_EQ(elements.size(), i);
  EXPECT_EQ(GetParam().num_elements_in_subtree, i);

  i = 0;
  for (auto& e : base::Reversed(*scene.GetUiElementByName(GetParam().root))) {
    EXPECT_GT(elements.size(), i);
    EXPECT_EQ(elements[elements.size() - i++ - 1lu]->id(), e.id());
  }
  EXPECT_EQ(elements.size(), i);
  EXPECT_EQ(GetParam().num_elements_in_subtree, i);

  i = 0;
  for (auto& e : base::Reversed(*const_cast<const UiElement*>(
           scene.GetUiElementByName(GetParam().root)))) {
    EXPECT_GT(elements.size(), i);
    EXPECT_EQ(elements[elements.size() - i++ - 1lu]->id(), e.id());
  }
  EXPECT_EQ(elements.size(), i);
  EXPECT_EQ(GetParam().num_elements_in_subtree, i);
}

const std::vector<UiElementIteratorTestCase> iterator_test_cases = {
    {kRoot, 8},
    {k2dBrowsingContentGroup, 4},
    {kCeiling, 1},
};

INSTANTIATE_TEST_CASE_P(UiElementIteratorTestCases,
                        UiElementIteratorTest,
                        ::testing::ValuesIn(iterator_test_cases));

}  // namespace vr
