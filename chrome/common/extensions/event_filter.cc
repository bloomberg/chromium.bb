// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/event_filter.h"

#include "chrome/common/extensions/matcher/url_matcher_factory.h"

namespace extensions {

EventFilter::EventMatcherEntry::EventMatcherEntry(
    scoped_ptr<EventMatcher> event_matcher,
    URLMatcher* url_matcher,
    const URLMatcherConditionSet::Vector& condition_sets)
    : event_matcher_(event_matcher.Pass()),
      url_matcher_(url_matcher) {
  for (URLMatcherConditionSet::Vector::const_iterator it =
       condition_sets.begin(); it != condition_sets.end(); it++)
    condition_set_ids_.push_back((*it)->id());
  url_matcher_->AddConditionSets(condition_sets);
}

EventFilter::EventMatcherEntry::~EventMatcherEntry() {
  url_matcher_->RemoveConditionSets(condition_set_ids_);
}

void EventFilter::EventMatcherEntry::DontRemoveConditionSetsInDestructor() {
  condition_set_ids_.clear();
}

EventFilter::EventFilter()
    : next_id_(0),
      next_condition_set_id_(0) {
}

EventFilter::~EventFilter() {
  // Normally when an event matcher entry is removed from event_matchers_ it
  // will remove its condition sets from url_matcher_, but as url_matcher_ is
  // being destroyed anyway there is no need to do that step here.
  for (EventMatcherMultiMap::iterator it = event_matchers_.begin();
       it != event_matchers_.end(); it++) {
    for (EventMatcherMap::iterator it2 = it->second.begin();
         it2 != it->second.end(); it2++) {
      it2->second->DontRemoveConditionSetsInDestructor();
    }
  }
}

EventFilter::MatcherID
EventFilter::AddEventMatcher(const std::string& event_name,
                             scoped_ptr<EventMatcher> matcher) {
  MatcherID id = next_id_++;
  URLMatcherConditionSet::Vector condition_sets;
  if (!CreateConditionSets(id, matcher->url_filters(), &condition_sets))
    return -1;

  for (URLMatcherConditionSet::Vector::iterator it = condition_sets.begin();
       it != condition_sets.end(); it++) {
    condition_set_id_to_event_matcher_id_.insert(
        std::make_pair((*it)->id(), id));
  }
  id_to_event_name_[id] = event_name;
  event_matchers_[event_name][id] = linked_ptr<EventMatcherEntry>(
      new EventMatcherEntry(matcher.Pass(), &url_matcher_, condition_sets));
  return id;
}

bool EventFilter::CreateConditionSets(
    MatcherID id,
    base::ListValue* url_filters,
    URLMatcherConditionSet::Vector* condition_sets) {
  if (!url_filters || url_filters->GetSize() == 0) {
    // If there are no url_filters then we want to match all events, so create a
    // URLFilter from an empty dictionary.
    base::DictionaryValue empty_dict;
    return AddDictionaryAsConditionSet(&empty_dict, condition_sets);
  }
  for (size_t i = 0; i < url_filters->GetSize(); i++) {
    base::DictionaryValue* url_filter;
    if (!url_filters->GetDictionary(i, &url_filter))
      return false;
    if (!AddDictionaryAsConditionSet(url_filter, condition_sets))
      return false;
  }
  return true;
}

bool EventFilter::AddDictionaryAsConditionSet(
    base::DictionaryValue* url_filter,
    URLMatcherConditionSet::Vector* condition_sets) {
  std::string error;
  URLMatcherConditionSet::ID condition_set_id = next_condition_set_id_++;
  condition_sets->push_back(URLMatcherFactory::CreateFromURLFilterDictionary(
      url_matcher_.condition_factory(),
      url_filter,
      condition_set_id,
      &error));
  if (!error.empty()) {
    LOG(ERROR) << "CreateFromURLFilterDictionary failed: " << error;
    url_matcher_.ClearUnusedConditionSets();
    condition_sets->clear();
    return false;
  }
  return true;
}

std::string EventFilter::RemoveEventMatcher(MatcherID id) {
  std::map<MatcherID, std::string>::iterator it = id_to_event_name_.find(id);
  event_matchers_[it->second].erase(id);
  std::string event_name = it->second;
  id_to_event_name_.erase(it);
  return event_name;
}

std::set<EventFilter::MatcherID> EventFilter::MatchEvent(
    const std::string& event_name, const EventFilteringInfo& event_info) {
  std::set<MatcherID> matchers;
  EventMatcherMultiMap::iterator it = event_matchers_.find(event_name);
  if (it == event_matchers_.end())
    return matchers;

  EventMatcherMap& matcher_map = it->second;
  GURL url_to_match_against = event_info.has_url() ? event_info.url() : GURL();
  std::set<URLMatcherConditionSet::ID> matching_condition_set_ids =
      url_matcher_.MatchURL(url_to_match_against);
  for (std::set<URLMatcherConditionSet::ID>::iterator it =
       matching_condition_set_ids.begin();
       it != matching_condition_set_ids.end(); it++) {
    MatcherID id = condition_set_id_to_event_matcher_id_[*it];
    const EventMatcher& event_matcher = matcher_map[id]->event_matcher();
    if (event_matcher.MatchNonURLCriteria(event_info)) {
      CHECK(!event_matcher.url_filters() || event_info.has_url());
      matchers.insert(id);
    }
  }

  return matchers;
}

int EventFilter::GetMatcherCountForEvent(const std::string& name) {
  EventMatcherMultiMap::const_iterator it = event_matchers_.find(name);
  if (it == event_matchers_.end())
    return 0;

  return it->second.size();
}

}  // namespace extensions
