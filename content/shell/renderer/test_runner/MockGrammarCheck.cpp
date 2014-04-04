// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockGrammarCheck.h"

#include <algorithm>

#include "base/logging.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"

using namespace blink;
using namespace std;

namespace WebTestRunner {

bool MockGrammarCheck::checkGrammarOfString(const WebString& text, vector<WebTextCheckingResult>* results)
{
    DCHECK(results);
    base::string16 stringText = text;
    if (find_if(stringText.begin(), stringText.end(), isASCIIAlpha) == stringText.end())
        return true;

    // Find matching grammatical errors from known ones. This function has to
    // check all errors because the given text may consist of two or more
    // sentences that have grammatical errors.
    static const struct {
        const char* text;
        int location;
        int length;
    } grammarErrors[] = {
        {"I have a issue.", 7, 1},
        {"I have an grape.", 7, 2},
        {"I have an kiwi.", 7, 2},
        {"I have an muscat.", 7, 2},
        {"You has the right.", 4, 3},
        {"apple orange zz.", 0, 16},
        {"apple zz orange.", 0, 16},
        {"apple,zz,orange.", 0, 16},
        {"orange,zz,apple.", 0, 16},
        {"the the adlj adaasj sdklj. there there", 4, 3},
        {"the the adlj adaasj sdklj. there there", 33, 5},
        {"zz apple orange.", 0, 16},
    };
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(grammarErrors); ++i) {
        size_t offset = 0;
        base::string16 error(grammarErrors[i].text, grammarErrors[i].text + strlen(grammarErrors[i].text));
        while ((offset = stringText.find(error, offset)) != base::string16::npos) {
            results->push_back(WebTextCheckingResult(WebTextDecorationTypeGrammar, offset + grammarErrors[i].location, grammarErrors[i].length));
            offset += grammarErrors[i].length;
        }
    }
    return false;
}

}
