// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/autocomplete/shortcuts_provider_shortcut.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/shortcuts_provider.h"

namespace {

// Takes Match classification vector and removes all matched positions,
// compacting repetitions if necessary.
void StripMatchMarkersFromClassifications(ACMatchClassifications* matches) {
  DCHECK(matches);
  ACMatchClassifications unmatched;
  for (ACMatchClassifications::iterator i = matches->begin();
       i != matches->end(); ++i) {
    shortcuts_provider::AddLastMatchIfNeeded(&unmatched, i->offset,
        i->style & ~ACMatchClassification::MATCH);
  }
  matches->swap(unmatched);
}

}  // namespace

namespace shortcuts_provider {

Shortcut::Shortcut(const string16& text,
                   const GURL& url,
                   const string16& contents,
                   const ACMatchClassifications& in_contents_class,
                   const string16& description,
                   const ACMatchClassifications& in_description_class)
    : text(text),
      url(url),
      contents(contents),
      contents_class(in_contents_class),
      description(description),
      description_class(in_description_class),
      last_access_time(base::Time::Now()),
      number_of_hits(1) {
  StripMatchMarkersFromClassifications(&contents_class);
  StripMatchMarkersFromClassifications(&description_class);
}

Shortcut::Shortcut(const std::string& id,
                   const string16& text,
                   const string16& url,
                   const string16& contents,
                   const string16& contents_class,
                   const string16& description,
                   const string16& description_class,
                   int64 time_of_last_access,
                   int   number_of_hits)
    : id(id),
      text(text),
      url(url),
      contents(contents),
      contents_class(SpansFromString(contents_class)),
      description(description),
      description_class(SpansFromString(description_class)),
      last_access_time(base::Time::FromInternalValue(time_of_last_access)),
      number_of_hits(1) {}

Shortcut::Shortcut()
    : last_access_time(base::Time::Now()),
      number_of_hits(0) {}

Shortcut::~Shortcut() {}

string16 Shortcut::contents_class_as_str() const {
  return SpansToString(contents_class);
}

string16 Shortcut::description_class_as_str() const {
  return SpansToString(description_class);
}

string16 SpansToString(const ACMatchClassifications& value) {
  string16 spans;
  string16 comma(ASCIIToUTF16(","));
  for (size_t i = 0; i < value.size(); ++i) {
    if (i)
      spans.append(comma);
    spans.append(base::IntToString16(value[i].offset));
    spans.append(comma);
    spans.append(base::IntToString16(value[i].style));
  }
  return spans;
}

ACMatchClassifications SpansFromString(const string16& value) {
  ACMatchClassifications spans;
  std::vector<string16> tokens;
  Tokenize(value, ASCIIToUTF16(","), &tokens);
  // The number of tokens should be even.
  if ((tokens.size() & 1) == 1) {
    NOTREACHED();
    return spans;
  }
  for (size_t i = 0; i < tokens.size(); i += 2) {
    int span_start = 0;
    int span_type = ACMatchClassification::NONE;
    if (!base::StringToInt(tokens[i], &span_start) ||
        !base::StringToInt(tokens[i + 1], &span_type)) {
      NOTREACHED();
      return spans;
    }
    spans.push_back(ACMatchClassification(span_start, span_type));
  }
  return spans;
}

// Adds match at the end if and only if its style is different from the last
// match.
void AddLastMatchIfNeeded(ACMatchClassifications* matches,
                          int position,
                          int style) {
  DCHECK(matches);
  if (matches->empty() || matches->back().style != style)
    matches->push_back(ACMatchClassification(position, style));
}

}  // namespace shortcuts_provider
