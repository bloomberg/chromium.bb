// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/ibus/ibus_component.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(IBusComponentTest, WriteReadIBusComponentTest) {
  const std::string kName = "Component Name";
  const std::string kDescription = "Component Description";
  const std::string kAuthor = "Component Author";

  const std::string kEngineId1 = "Engine Id 1";
  const std::string kEngineDisplayName1 = "Engine Display Name 1";
  const std::string kEngineDescription1 = "Engine Description 1";
  const std::string kEngineLanguageCode1 = "en";
  const std::string kEngineAuthor1 = "Engine Author 1";
  const std::string kEngineLayout1 = "us::eng";
  const IBusComponent::EngineDescription engine_desc1(kEngineId1,
                                                      kEngineDisplayName1,
                                                      kEngineDescription1,
                                                      kEngineLanguageCode1,
                                                      kEngineAuthor1,
                                                      kEngineLayout1);

  const std::string kEngineId2 = "Engine Id 2";
  const std::string kEngineDisplayName2 = "Engine Display Name 2";
  const std::string kEngineDescription2 = "Engine Description 2";
  const std::string kEngineLanguageCode2 = "ja";
  const std::string kEngineAuthor2 = "Engine Author 2";
  const std::string kEngineLayout2 = "ja::jp";
  const IBusComponent::EngineDescription engine_desc2(kEngineId2,
                                                      kEngineDisplayName2,
                                                      kEngineDescription2,
                                                      kEngineLanguageCode2,
                                                      kEngineAuthor2,
                                                      kEngineLayout2);

  // Create a IBusComponent.
  IBusComponent ibus_component;
  ibus_component.set_name(kName);
  ibus_component.set_description(kDescription);
  ibus_component.set_author(kAuthor);
  ibus_component.mutable_engine_description()->push_back(engine_desc1);
  ibus_component.mutable_engine_description()->push_back(engine_desc2);

  // Write a IBusComponent.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendIBusComponent(ibus_component, &writer);

  // Read a IBusComponent.
  IBusComponent target_component;
  dbus::MessageReader reader(response.get());
  PopIBusComponent(&reader, &target_component);

  // Check a result.
  EXPECT_EQ(kName, target_component.name());
  EXPECT_EQ(kDescription, target_component.description());
  EXPECT_EQ(kAuthor, target_component.author());

  const std::vector<IBusComponent::EngineDescription>& engine_descriptions =
      ibus_component.engine_description();
  EXPECT_EQ(kEngineId1, engine_descriptions[0].engine_id);
  EXPECT_EQ(kEngineDisplayName1, engine_descriptions[0].display_name);
  EXPECT_EQ(kEngineDescription1, engine_descriptions[0].description);
  EXPECT_EQ(kEngineLanguageCode1, engine_descriptions[0].language_code);
  EXPECT_EQ(kEngineAuthor1, engine_descriptions[0].author);
  EXPECT_EQ(kEngineLayout1, engine_descriptions[0].layout);

  EXPECT_EQ(kEngineId2, engine_descriptions[1].engine_id);
  EXPECT_EQ(kEngineDisplayName2, engine_descriptions[1].display_name);
  EXPECT_EQ(kEngineDescription2, engine_descriptions[1].description);
  EXPECT_EQ(kEngineLanguageCode2, engine_descriptions[1].language_code);
  EXPECT_EQ(kEngineAuthor2, engine_descriptions[1].author);
  EXPECT_EQ(kEngineLayout2, engine_descriptions[1].layout);
}

}  // namespace chromeos
