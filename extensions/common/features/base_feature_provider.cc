// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/base_feature_provider.h"

#include <stack>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/simple_feature.h"

namespace extensions {

namespace {

bool ParseFeature(const base::DictionaryValue* value,
                  const std::string& name,
                  SimpleFeature* feature) {
  feature->set_name(name);
  std::string error = feature->Parse(value);
  if (!error.empty())
    LOG(ERROR) << error;
  return error.empty();
}

}  // namespace

BaseFeatureProvider::BaseFeatureProvider(const base::DictionaryValue& root,
                                         FeatureFactory factory)
    : factory_(factory) {
  for (base::DictionaryValue::Iterator iter(root); !iter.IsAtEnd();
       iter.Advance()) {
    if (iter.value().GetType() == base::Value::TYPE_DICTIONARY) {
      linked_ptr<SimpleFeature> feature((*factory_)());

      std::vector<std::string> split;
      base::SplitString(iter.key(), '.', &split);

      // Push parent features on the stack, starting with the current feature.
      // If one of the features has "noparent" set, stop pushing features on
      // the stack. The features will then be parsed in order, starting with
      // the farthest parent that is either top level or has "noparent" set.
      std::stack<std::pair<std::string, const base::DictionaryValue*> >
          parse_stack;
      while (!split.empty()) {
        std::string parent_name = JoinString(split, '.');
        split.pop_back();
        if (root.HasKey(parent_name)) {
          const base::DictionaryValue* parent = NULL;
          CHECK(root.GetDictionaryWithoutPathExpansion(parent_name, &parent));
          parse_stack.push(std::make_pair(parent_name, parent));
          bool no_parent = false;
          parent->GetBoolean("noparent", &no_parent);
          if (no_parent)
            break;
        }
      }

      CHECK(!parse_stack.empty());
      // Parse all parent features.
      bool parse_error = false;
      while (!parse_stack.empty()) {
        if (!ParseFeature(parse_stack.top().second,
                          parse_stack.top().first,
                          feature.get())) {
          parse_error = true;
          break;
        }
        parse_stack.pop();
      }

      if (parse_error)
        continue;

      features_[iter.key()] = feature;
    } else if (iter.value().GetType() == base::Value::TYPE_LIST) {
      // This is a complex feature.
      const base::ListValue* list =
          static_cast<const base::ListValue*>(&iter.value());
      CHECK_GT(list->GetSize(), 0UL);

      scoped_ptr<ComplexFeature::FeatureList> features(
          new ComplexFeature::FeatureList());

      // Parse and add all SimpleFeatures from the list.
      for (base::ListValue::const_iterator list_iter = list->begin();
           list_iter != list->end(); ++list_iter) {
        if ((*list_iter)->GetType() != base::Value::TYPE_DICTIONARY) {
          LOG(ERROR) << iter.key() << ": Feature rules must be dictionaries.";
          continue;
        }

        scoped_ptr<SimpleFeature> feature((*factory_)());
        if (!ParseFeature(static_cast<const base::DictionaryValue*>(*list_iter),
                          iter.key(),
                          feature.get()))
          continue;

        features->push_back(feature.release());
      }

      linked_ptr<ComplexFeature> feature(new ComplexFeature(features.Pass()));
      feature->set_name(iter.key());

      features_[iter.key()] = feature;
    } else {
      LOG(ERROR) << iter.key() << ": Feature description must be dictionary or"
                 << " list of dictionaries.";
    }
  }
}

BaseFeatureProvider::~BaseFeatureProvider() {
}

const std::vector<std::string>& BaseFeatureProvider::GetAllFeatureNames()
    const {
  if (feature_names_.empty()) {
    for (FeatureMap::const_iterator iter = features_.begin();
         iter != features_.end(); ++iter) {
      feature_names_.push_back(iter->first);
    }
    // A std::map is sorted by its keys, so we don't need to sort feature_names_
    // now.
  }
  return feature_names_;
}

Feature* BaseFeatureProvider::GetFeature(const std::string& name) const {
  FeatureMap::const_iterator iter = features_.find(name);
  if (iter != features_.end())
    return iter->second.get();
  else
    return NULL;
}

Feature* BaseFeatureProvider::GetParent(Feature* feature) const {
  CHECK(feature);
  if (feature->no_parent())
    return NULL;

  std::vector<std::string> split;
  base::SplitString(feature->name(), '.', &split);
  if (split.size() < 2)
    return NULL;
  split.pop_back();
  return GetFeature(JoinString(split, '.'));
}

// Children of a given API are named starting with parent.name()+".", which
// means they'll be contiguous in the features_ std::map.
std::vector<Feature*> BaseFeatureProvider::GetChildren(const Feature& parent)
    const {
  std::string prefix = parent.name() + ".";
  const FeatureMap::const_iterator first_child = features_.lower_bound(prefix);

  // All children have names before (parent.name() + ('.'+1)).
  ++prefix[prefix.size() - 1];
  const FeatureMap::const_iterator after_children =
      features_.lower_bound(prefix);

  std::vector<Feature*> result;
  result.reserve(std::distance(first_child, after_children));
  for (FeatureMap::const_iterator it = first_child; it != after_children;
       ++it) {
    result.push_back(it->second.get());
  }
  return result;
}

}  // namespace extensions
