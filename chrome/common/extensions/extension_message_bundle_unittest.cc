// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_message_bundle.h"

#include <string>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper method for dictionary building.
void SetDictionary(const std::wstring name,
                   DictionaryValue* target,
                   DictionaryValue* subtree) {
  target->Set(name, static_cast<Value*>(subtree));
}

void CreateContentTree(const std::wstring& name,
                       const std::string content,
                       DictionaryValue* dict) {
  DictionaryValue* content_tree = new DictionaryValue;
  content_tree->SetString(ExtensionMessageBundle::kContentKey, content);
  SetDictionary(name, dict, content_tree);
}

void CreatePlaceholdersTree(DictionaryValue* dict) {
  DictionaryValue* placeholders_tree = new DictionaryValue;
  CreateContentTree(L"a", "A", placeholders_tree);
  CreateContentTree(L"b", "B", placeholders_tree);
  CreateContentTree(L"c", "C", placeholders_tree);
  SetDictionary(ExtensionMessageBundle::kPlaceholdersKey,
                dict,
                placeholders_tree);
}

void CreateMessageTree(const std::wstring& name,
                       const std::string& message,
                       bool create_placeholder_subtree,
                       DictionaryValue* dict) {
  DictionaryValue* message_tree = new DictionaryValue;
  if (create_placeholder_subtree)
    CreatePlaceholdersTree(message_tree);
  message_tree->SetString(ExtensionMessageBundle::kMessageKey, message);
  SetDictionary(name, dict, message_tree);
}

void CreateGoodDictionary(DictionaryValue* dict) {
  dict->Clear();
  CreateMessageTree(L"n1", "message1 $a$ $b$", true, dict);
  CreateMessageTree(L"n2", "message2 $c$", true, dict);
  CreateMessageTree(L"n3", "message3", false, dict);
}

enum BadDictionary {
  INVALID_NAME,
  NAME_NOT_A_TREE,
  EMPTY_NAME_TREE,
  MISSING_MESSAGE,
  PLACEHOLDER_NOT_A_TREE,
  EMPTY_PLACEHOLDER_TREE,
  CONTENT_MISSING,
  MESSAGE_PLACEHOLDER_DOESNT_MATCH,
};

void CreateBadDictionary(DictionaryValue* dict,
                         enum BadDictionary what_is_bad) {
  CreateGoodDictionary(dict);
  // Now remove/break things.
  switch (what_is_bad) {
    case INVALID_NAME:
      CreateMessageTree(L"n 5", "nevermind", false, dict);
      break;
    case NAME_NOT_A_TREE:
      dict->SetString(L"n4", "whatever");
      break;
    case EMPTY_NAME_TREE: {
        DictionaryValue* empty_tree = new DictionaryValue;
        SetDictionary(L"n4", dict, empty_tree);
      }
      break;
    case MISSING_MESSAGE:
      dict->Remove(L"n1.message", NULL);
      break;
    case PLACEHOLDER_NOT_A_TREE:
      dict->SetString(L"n1.placeholders", "whatever");
      break;
    case EMPTY_PLACEHOLDER_TREE: {
        DictionaryValue* empty_tree = new DictionaryValue;
        SetDictionary(L"n1.placeholders", dict, empty_tree);
      }
      break;
    case CONTENT_MISSING:
       dict->Remove(L"n1.placeholders.a.content", NULL);
      break;
    case MESSAGE_PLACEHOLDER_DOESNT_MATCH:
      DictionaryValue* value;
      dict->Remove(L"n1.placeholders.a", NULL);
      dict->GetDictionary(L"n1.placeholders", &value);
      CreateContentTree(L"x", "X", value);
      break;
  }
}

TEST(ExtensionMessageBundle, InitEmptyDictionaries) {
  DictionaryValue default_dict;
  DictionaryValue app_dict;
  std::string error;
  scoped_ptr<ExtensionMessageBundle> handler(
      ExtensionMessageBundle::Create(default_dict, app_dict, &error));

  EXPECT_TRUE(handler.get() != NULL);
  EXPECT_EQ(0U, handler->size());
}

TEST(ExtensionMessageBundle, InitGoodDefaultDictEmptyAppDict) {
  DictionaryValue default_dict;
  DictionaryValue app_dict;
  std::string error;

  CreateGoodDictionary(&default_dict);
  scoped_ptr<ExtensionMessageBundle> handler(
      ExtensionMessageBundle::Create(default_dict, app_dict, &error));

  EXPECT_TRUE(handler.get() != NULL);
  EXPECT_EQ(3U, handler->size());

  EXPECT_EQ("message1 A B", handler->GetL10nMessage("n1"));
  EXPECT_EQ("message2 C", handler->GetL10nMessage("n2"));
  EXPECT_EQ("message3", handler->GetL10nMessage("n3"));
}

TEST(ExtensionMessageBundle, InitAppDictConsultedFirst) {
  DictionaryValue default_dict;
  DictionaryValue app_dict;
  std::string error;

  CreateGoodDictionary(&default_dict);
  CreateGoodDictionary(&app_dict);
  // Flip placeholders in message of n1 tree.
  app_dict.SetString(L"n1.message", "message1 $b$ $a$");
  // Remove one message from app dict.
  app_dict.Remove(L"n2", NULL);
  // Replace n3 with N3.
  app_dict.Remove(L"n3", NULL);
  CreateMessageTree(L"N3", "message3_app_dict", false, &app_dict);

  scoped_ptr<ExtensionMessageBundle> handler(
      ExtensionMessageBundle::Create(default_dict, app_dict, &error));

  EXPECT_TRUE(handler.get() != NULL);
  EXPECT_EQ(3U, handler->size());

  EXPECT_EQ("message1 B A", handler->GetL10nMessage("n1"));
  EXPECT_EQ("message2 C", handler->GetL10nMessage("n2"));
  EXPECT_EQ("message3_app_dict", handler->GetL10nMessage("n3"));
}

TEST(ExtensionMessageBundle, InitBadAppDict) {
  DictionaryValue default_dict;
  DictionaryValue app_dict;
  std::string error;

  CreateBadDictionary(&app_dict, INVALID_NAME);
  scoped_ptr<ExtensionMessageBundle> handler(
      ExtensionMessageBundle::Create(default_dict, app_dict, &error));

  EXPECT_TRUE(handler.get() == NULL);
  EXPECT_EQ("Name of a key \"n 5\" is invalid. Only ASCII [a-z], "
            "[A-Z], [0-9] and \"_\" are allowed.", error);

  CreateBadDictionary(&app_dict, NAME_NOT_A_TREE);
  handler.reset(ExtensionMessageBundle::Create(default_dict, app_dict, &error));
  EXPECT_TRUE(handler.get() == NULL);
  EXPECT_EQ("Not a valid tree for key n4.", error);

  CreateBadDictionary(&app_dict, EMPTY_NAME_TREE);
  handler.reset(ExtensionMessageBundle::Create(default_dict, app_dict, &error));
  EXPECT_TRUE(handler.get() == NULL);
  EXPECT_EQ("There is no \"message\" element for key n4.", error);

  CreateBadDictionary(&app_dict, MISSING_MESSAGE);
  handler.reset(ExtensionMessageBundle::Create(default_dict, app_dict, &error));
  EXPECT_TRUE(handler.get() == NULL);
  EXPECT_EQ("There is no \"message\" element for key n1.", error);

  CreateBadDictionary(&app_dict, PLACEHOLDER_NOT_A_TREE);
  handler.reset(ExtensionMessageBundle::Create(default_dict, app_dict, &error));
  EXPECT_TRUE(handler.get() == NULL);
  EXPECT_EQ("Not a valid \"placeholders\" element for key n1.", error);

  CreateBadDictionary(&app_dict, EMPTY_PLACEHOLDER_TREE);
  handler.reset(ExtensionMessageBundle::Create(default_dict, app_dict, &error));
  EXPECT_TRUE(handler.get() == NULL);
  EXPECT_EQ("Variable $a$ used but not defined.", error);

  CreateBadDictionary(&app_dict, CONTENT_MISSING);
  handler.reset(ExtensionMessageBundle::Create(default_dict, app_dict, &error));
  EXPECT_TRUE(handler.get() == NULL);
  EXPECT_EQ("Invalid \"content\" element for key n1.", error);

  CreateBadDictionary(&app_dict, MESSAGE_PLACEHOLDER_DOESNT_MATCH);
  handler.reset(ExtensionMessageBundle::Create(default_dict, app_dict, &error));
  EXPECT_TRUE(handler.get() == NULL);
  EXPECT_EQ("Variable $a$ used but not defined.", error);
}

struct ReplaceVariables {
  const char* original;
  const char* result;
  const char* error;
  const char* begin_delimiter;
  const char* end_delimiter;
  bool pass;
};

TEST(ExtensionMessageBundle, ReplaceMessagesInText) {
  const char* kMessageBegin = ExtensionMessageBundle::kMessageBegin;
  const char* kMessageEnd = ExtensionMessageBundle::kMessageEnd;
  const char* kPlaceholderBegin = ExtensionMessageBundle::kPlaceholderBegin;
  const char* kPlaceholderEnd = ExtensionMessageBundle::kPlaceholderEnd;

  static ReplaceVariables test_cases[] = {
    // Message replacement.
    { "This is __MSG_siMPle__ message", "This is simple message",
      "", kMessageBegin, kMessageEnd, true },
    { "This is __MSG_", "This is __MSG_",
      "", kMessageBegin, kMessageEnd, true },
    { "This is __MSG__simple__ message", "This is __MSG__simple__ message",
      "Variable __MSG__simple__ used but not defined.",
      kMessageBegin, kMessageEnd, false },
    { "__MSG_LoNg__", "A pretty long replacement",
      "", kMessageBegin, kMessageEnd, true },
    { "A __MSG_SimpLE__MSG_ a", "A simpleMSG_ a",
      "", kMessageBegin, kMessageEnd, true },
    { "A __MSG_simple__MSG_long__", "A simpleMSG_long__",
      "", kMessageBegin, kMessageEnd, true },
    { "A __MSG_simple____MSG_long__", "A simpleA pretty long replacement",
      "", kMessageBegin, kMessageEnd, true },
    { "__MSG_d1g1ts_are_ok__", "I are d1g1t",
      "", kMessageBegin, kMessageEnd, true },
    // Placeholder replacement.
    { "This is $sImpLe$ message", "This is simple message",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "This is $", "This is $",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "This is $$sIMPle$ message", "This is $simple message",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "$LONG_V$", "A pretty long replacement",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "A $simple$$ a", "A simple$ a",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "A $simple$long_v$", "A simplelong_v$",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "A $simple$$long_v$", "A simpleA pretty long replacement",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "This is $bad name$", "This is $bad name$",
       "", kPlaceholderBegin, kPlaceholderEnd, true },
    { "This is $missing$", "This is $missing$",
       "Variable $missing$ used but not defined.",
       kPlaceholderBegin, kPlaceholderEnd, false },
  };

  ExtensionMessageBundle::SubstitutionMap messages;
  messages.insert(std::make_pair("simple", "simple"));
  messages.insert(std::make_pair("long", "A pretty long replacement"));
  messages.insert(std::make_pair("long_v", "A pretty long replacement"));
  messages.insert(std::make_pair("bad name", "Doesn't matter"));
  messages.insert(std::make_pair("d1g1ts_are_ok", "I are d1g1t"));

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    std::string text = test_cases[i].original;
    std::string error;
    EXPECT_EQ(test_cases[i].pass,
      ExtensionMessageBundle::ReplaceVariables(messages,
                                               test_cases[i].begin_delimiter,
                                               test_cases[i].end_delimiter,
                                               &text,
                                               &error));
    EXPECT_EQ(test_cases[i].result, text);
  }
}

}  // namespace
