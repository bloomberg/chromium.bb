#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Reverses the text of an HTML file.

This classes poorly parses an HTML file and reverse the text strings (and only
the text, not the tags).
It is used to generates the _TRANSLATED.html files that the translator unittest
uses.
Note it is very hacky and buggy.
"""

import codecs
import re
import sys

def Error(msg):
  print msg;
  sys.exit(1);

class HTMLParser:
  # IGNORED_[PAIRED|SINGLE]_TAGS should be kept in sync with kSkippedTags (see
  # chrome/renderer/translator.cc).
  # Paired tags are tags that are expected to have an opening and closing tag,
  # the entire zone they contain is ignored.
  # Single tags are not closed and are ignored.
  IGNORED_PAIRED_TAGS = [ "APPLET", "AREA", "BASE", "FRAME", "FRAMESET", "HR",
                          "IFRAME", "MAP", "OBJECT", "PARAM", "SCRIPT", "STYLE",
                          "TEXTAREA" ];
  IGNORED_SINGLE_TAGS = [ "META", "LINK", "IMG", "INPUT" ];

  def __init__(self, input_path, output_path):
    try:
      input_file = codecs.open(input_path, 'r', 'utf-8');
    except IOError:
      Error("Failed to open '" +  input_path + "' for reading.");

    self.html_contents = input_file.read();
    # Python does not have a find method case-insensitive, so we keep a lower
    # case copy of the contents.
    self.html_contents_lower = self.html_contents.lower();

    input_file.close();

    self.read_index = 0
    self.write_index = 0
    try:
      self.output_file = codecs.open(output_path, 'w', 'utf-8');
    except IOError:
      Error("Failed to open '" + output_path + "' for writting.");

  def printDebug(self, msg):
    print u"** %s" % msg.encode('ascii', 'replace')

  def removeBlanks(self, str):
    p = re.compile('\s');
    return p.sub('', str);

  def extractTagName(self, str):
    closing_tag = False;
    str = str.strip();
    if str[0] != "<":
      Error("Interal error: attempting to extract tag name from invalid tag: " +
            str);
    if str[1] == "/":
      closing_tag = True;

    p = re.compile('</?\s*(\w*).*');
    m = p.match(str);
    if m == None:
      Error("Interal error: failed to extract tag name from tag: " + str);
    return (m.group(1).lower(), closing_tag);

  def shouldIgnoreTag(self, tag):
    """Returns a tuple (tag should be ignored, pared tags)
    """
    tag = tag.upper();
    for tag_to_ignore in self.IGNORED_PAIRED_TAGS:
      if tag_to_ignore == tag:
        return True, True;
    for tag_to_ignore in self.IGNORED_SINGLE_TAGS:
      if tag_to_ignore == tag:
        return True, False;
    return False, False;

  def skipToEndTag(self, tag):
    """ Move the read_index to the position after the closing tag matching
        |tag| and copies all the skipped data to the output file."""
    index = self.html_contents_lower.find("</" + tag, self.read_index);
    if index == -1:
      Error("Failed to find tag end for tag " + tag + " at index " +
            str(self.read_index));
      self.writeToOutputFile(self.html_contents[self.read_index:]);
    else:
      self.writeToOutputFile(self.html_contents[self.read_index:index]);
      self.read_index = index;

  def writeToOutputFile(self, text):
    try:
      self.output_file.write(text)
    except IOError:
      Error("Failed to write to output file.");
    # DEBUG
    if len(text) > 100000:
      Error("Writting too much text: " + text);
#    self.printDebug("Writting: " + text);
#    self.write_index += len(text);
#    self.printDebug("Wrote " + str(len(text)) + " bytes, write len=" + str(self.write_index));

  def getNextTag(self):
    """Moves the read_index to the end of the next tag and writes the tag to the
    output file.
    Returns a tuple end of file reached, tag name, if closing tag.
    """

    start_index = self.html_contents.find("<", self.read_index);
    if start_index == -1:
      self.writeToOutputFile(self.html_contents[self.read_index:]);
      return (True, "", False);
    stop_index = self.html_contents.find(">", start_index);
    if stop_index == -1:
      print "Unclosed tag found.";
      self.writeToOutputFile(self.html_contents[self.read_index:]);
      return (True, "", False);

    # Write to the file the current text reverted.
    # No need to do it if the string is only blanks, that would break the
    # indentation.
    text = self.html_contents[self.read_index:start_index]
    text = self.processText(text);
    self.writeToOutputFile(text);

    tag = self.html_contents[start_index:stop_index + 1];
    self.writeToOutputFile(tag);
    self.read_index = stop_index + 1;
    tag_name, closing_tag = self.extractTagName(tag);
#    self.printDebug("Raw tag=" + tag);
#    self.printDebug("tag=" + tag_name + " closing=" + str(closing_tag));
#    self.printDebug("read_index=" + str(self.read_index));

    return (False, tag_name, closing_tag);

  def processText(self, text):
    if text.isspace():
      return text;

    # Special case of lonely &nbsp; with spaces. It should not be reversed as
    # the renderer does not "translate" it as it is seen as empty string.
    if text.strip().lower() == '&nbsp;':
      return text;

    # We reverse the string manually so to preserve &nbsp; and friends.
    p = re.compile(r'&#\d{1,5};|&\w{2,6};');
    # We create a dictionary where the key is the index at which the ASCII code
    # starts and the value the index at which it ends.
    entityNameIndexes = dict();
    for match in p.finditer(text):
      entityNameIndexes[match.start()] = match.end();
    result = ""
    i = 0;
    while i < len(text):
      if entityNameIndexes.has_key(i):
        end_index = entityNameIndexes[i];
        result = text[i:end_index] + result;
        i = end_index;
      elif text[i] == "%":  # Replace percent to avoid percent encoding.
        result = "&#37;" + result;
        i = i + 1;
      else:
        result = text[i] + result;
        i = i + 1;

    return result;

  def processTagContent(self):
    """Reads the text from the current index to the next tag and writes the text
    in reverse to the output file.
    """
    stop_index = self.html_contents.find("<", self.read_index);
    if stop_index == -1:
      text = self.html_contents[self.read_index:];
      self.read_index += len(text);
    else:
      text = self.html_contents[self.read_index:stop_index];
      self.read_index = stop_index;
    text = self.processText(text);
    self.writeToOutputFile(text);

  def start(self):
    while True:
      end_of_file, tag, closing_tag = self.getNextTag();
      # if closing_tag:
      #   self.printDebug("Read tag: /" + tag);
      # else:
      #   self.printDebug("Read tag: " + tag);

      if end_of_file:  # We reached the end of the file.
        self.writeToOutputFile(self.html_contents[self.read_index:]);
        print "Done.";
        sys.exit(0);

      if closing_tag:
        continue;

      ignore_tag, paired_tag = self.shouldIgnoreTag(tag);
      if ignore_tag and paired_tag:
        self.skipToEndTag(tag);

      # Read and reverse the text in the tab.
      self.processTagContent();

def main():
  if len(sys.argv) != 3:
    Error("Reverse the text in HTML pages\n"
          "Usage reversetext.py <original_file.html> <dest_file.html>");

  html_parser = HTMLParser(sys.argv[1], sys.argv[2]);
  html_parser.start();

if __name__ == "__main__":
    main()
