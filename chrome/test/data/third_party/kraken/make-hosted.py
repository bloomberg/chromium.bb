#!/usr/bin/python

# Copyright (C) 2007 Apple Inc.  All rights reserved.
# Copyright (C) 2010 Mozilla Foundation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import with_statement
import os
import shutil

suites = ["kraken-1.1", "kraken-1.0", "sunspider-0.9.1"]

def readTemplate(path):
    with open(path, 'r') as f:
        return f.read()

template = readTemplate("resources/TEMPLATE.html")
driverTemplate = readTemplate("resources/driver-TEMPLATE.html")
resultsTemplate = readTemplate("resources/results-TEMPLATE.html")

def testListForSuite(suite):
    tests = []
    with open("./tests/%s/LIST" % suite, "r") as f:
        for line in f.readlines():
            tests.append(line.strip())
    return tests

def categoriesFromTests(tests):
    categories = set()
    for test in tests:
        categories.add(test[:test.find("-")])
    categories = list(categories)
    categories.sort()
    return categories

def escapeTestContent(test,suite):
    with open("tests/" + suite + "/" + test + ".js") as f:
        script = f.read()
    output = template
    output = output.replace("@NAME@", test)
    output = output.replace("@SCRIPT@", script)
    dataPath = "tests/" + suite + "/" + test + "-data.js"
    if (os.path.exists(dataPath)):
        with open(dataPath) as f:
            datascript = f.read()
        output = output.replace("@DATASCRIPT@", datascript)
    datascript = None
    output = output.replace("\\", "\\\\")
    output = output.replace('"', '\\"')
    output = output.replace("\n", "\\n\\\n")
    return output

def testContentsFromTests(suite, tests):
    testContents = [];
    for test in tests:
        testContents.append(escapeTestContent(test, suite))
    return testContents

def writeTemplate(suite, template, fileName):
    output = template.replace("@SUITE@", suite)
    with open("hosted/" + suite + "/" + fileName, "w") as f:
        f.write(output)

for suite in suites:
    suiteDir = os.path.join("hosted", suite)
    if not os.path.exists(suiteDir):
        os.mkdir(suiteDir)
    tests = testListForSuite(suite)
    categories = categoriesFromTests(tests)
    testContents = testContentsFromTests(suite, tests)
    writeTemplate(suite, driverTemplate, "driver.html")
    writeTemplate(suite, resultsTemplate, "results.html")

    prefix = "var tests = [ " + ", ".join(['"%s"' % s for s in tests]) + " ];\n"
    prefix += "var categories = [ " + ", ".join(['"%s"' % s for s in categories]) + " ];\n"
    with open("hosted/" + suite + "/test-prefix.js", "w") as f:
        f.write(prefix)

    contents = "var testContents = [ " + ", ".join(['"%s"' % s for s in testContents]) + " ];\n"
    with open("hosted/" + suite + "/test-contents.js", "w") as f:
        f.write(contents)

shutil.copyfile("resources/analyze-results.js", "hosted/analyze-results.js")
shutil.copyfile("resources/compare-results.js", "hosted/compare-results.js")

print("You're awesome!")
