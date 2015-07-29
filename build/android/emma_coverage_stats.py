#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates incremental code coverage reports for Java code in Chromium."""

import collections
import os
from xml.etree import ElementTree

NOT_EXECUTABLE = -1
NOT_COVERED = 0
COVERED = 1
PARTIALLY_COVERED = 2

# Coverage information about a single line of code.
LineCoverage = collections.namedtuple(
    'LineCoverage',
    ['lineno', 'source', 'covered_status', 'fractional_line_coverage'])


class _EmmaHtmlParser(object):
  """Encapsulates HTML file parsing operations.

  This class contains all operations related to parsing HTML files that were
  produced using the EMMA code coverage tool.

  Example HTML:

  Package links:
    <a href="_files/1.html">org.chromium.chrome</a>
    This is returned by the selector |XPATH_SELECT_PACKAGE_ELEMENTS|.

  Class links:
    <a href="1e.html">DoActivity.java</a>
    This is returned by the selector |XPATH_SELECT_CLASS_ELEMENTS|.

  Line coverage data:
    <tr class="p">
       <td class="l" title="78% line coverage (7 out of 9)">108</td>
       <td title="78% line coverage (7 out of 9 instructions)">
         if (index < 0 || index = mSelectors.size()) index = 0;</td>
    </tr>
    <tr>
       <td class="l">109</td>
       <td> </td>
    </tr>
    <tr class="c">
       <td class="l">110</td>
       <td>        if (mSelectors.get(index) != null) {</td>
    </tr>
    <tr class="z">
       <td class="l">111</td>
       <td>            for (int i = 0; i < mSelectors.size(); i++) {</td>
    </tr>
    Each <tr> element is returned by the selector |XPATH_SELECT_LOC|.

    We can parse this to get:
      1. Line number
      2. Line of source code
      3. Coverage status (c, z, or p)
      4. Fractional coverage value (% out of 100 if PARTIALLY_COVERED)
  """
  # Selector to match all <a> elements within the rows that are in the table
  # that displays all of the different packages.
  _XPATH_SELECT_PACKAGE_ELEMENTS = './/BODY/TABLE[4]/TR/TD/A'

  # Selector to match all <a> elements within the rows that are in the table
  # that displays all of the different packages within a class.
  _XPATH_SELECT_CLASS_ELEMENTS = './/BODY/TABLE[3]/TR/TD/A'

  # Selector to match all <tr> elements within the table containing Java source
  # code in an EMMA HTML file.
  _XPATH_SELECT_LOC = './/BODY/TABLE[4]/TR'

  # Children of HTML elements are represented as a list in ElementTree. These
  # constants represent list indices corresponding to relevant child elements.

  # Child 1 contains percentage covered for a line.
  _ELEMENT_PERCENT_COVERED = 1

  # Child 1 contains the original line of source code.
  _ELEMENT_CONTAINING_SOURCE_CODE = 1

  # Child 0 contains the line number.
  _ELEMENT_CONTAINING_LINENO = 0

  # Maps CSS class names to corresponding coverage constants.
  _CSS_TO_STATUS = {'c': COVERED, 'p': PARTIALLY_COVERED, 'z': NOT_COVERED}

  # UTF-8 no break space.
  _NO_BREAK_SPACE = '\xc2\xa0'

  def __init__(self, emma_file_base_dir):
    """Initializes _EmmaHtmlParser.

    Args:
      emma_file_base_dir: Path to the location where EMMA report files are
        stored. Should be where index.html is stored.
    """
    self._base_dir = emma_file_base_dir
    self._emma_files_path = os.path.join(self._base_dir, '_files')
    self._index_path = os.path.join(self._base_dir, 'index.html')

  def GetLineCoverage(self, emma_file_path):
    """Returns a list of LineCoverage objects for the given EMMA HTML file.

    Args:
      emma_file_path: String representing the path to the EMMA HTML file.

    Returns:
      A list of LineCoverage objects.
    """
    line_tr_elements = self._FindElements(
        emma_file_path, self._XPATH_SELECT_LOC)
    line_coverage = []
    for tr in line_tr_elements:
      # Get the coverage status.
      coverage_status = self._CSS_TO_STATUS.get(tr.get('CLASS'), NOT_EXECUTABLE)
      # Get the fractional coverage value.
      if coverage_status == PARTIALLY_COVERED:
        title_attribute = (tr[self._ELEMENT_PERCENT_COVERED].get('TITLE'))
        # Parse string that contains percent covered: "83% line coverage ...".
        percent_covered = title_attribute.split('%')[0]
        fractional_coverage = int(percent_covered) / 100.0
      else:
        fractional_coverage = 1.0

      # Get the line number.
      lineno_element = tr[self._ELEMENT_CONTAINING_LINENO]
      # Handles oddly formatted HTML (where there is an extra <a> tag).
      lineno = int(lineno_element.text or
                   lineno_element[self._ELEMENT_CONTAINING_LINENO].text)
      # Get the original line of Java source code.
      raw_source = tr[self._ELEMENT_CONTAINING_SOURCE_CODE].text
      utf8_source = raw_source.encode('UTF-8')
      source = utf8_source.replace(self._NO_BREAK_SPACE, ' ')

      line = LineCoverage(lineno, source, coverage_status, fractional_coverage)
      line_coverage.append(line)

    return line_coverage

  def GetPackageNameToEmmaFileDict(self):
    """Returns a dict mapping Java packages to EMMA HTML coverage files.

    Parses the EMMA index.html file to get a list of packages, then parses each
    package HTML file to get a list of classes for that package, and creates
    a dict with this info.

    Returns:
      A dict mapping string representation of Java packages (with class
        names appended) to the corresponding file paths of EMMA HTML files.
    """
    # These <a> elements contain each package name and the path of the file
    # where all classes within said package are listed.
    package_link_elements = self._FindElements(
        self._index_path, self._XPATH_SELECT_PACKAGE_ELEMENTS)
    # Maps file path of package directory (EMMA generated) to package name.
    # Ex. emma_dir/f.html: org.chromium.chrome.
    package_links = {
      os.path.join(self._base_dir, link.attrib['HREF']): link.text
      for link in package_link_elements if 'HREF' in link.attrib
    }

    package_to_emma = {}
    for package_emma_file_path, package_name in package_links.iteritems():
      # These <a> elements contain each class name in the current package and
      # the path of the file where the coverage info is stored for each class.
      coverage_file_link_elements = self._FindElements(
          package_emma_file_path, self._XPATH_SELECT_CLASS_ELEMENTS)

      for coverage_file_element in coverage_file_link_elements:
        emma_coverage_file_path = os.path.join(
            self._emma_files_path, coverage_file_element.attrib['HREF'])
        full_package_name = '%s.%s' % (package_name, coverage_file_element.text)
        package_to_emma[full_package_name] = emma_coverage_file_path

    return package_to_emma

  def _FindElements(self, file_path, xpath_selector):
    """Reads a HTML file and performs an XPath match.

    Args:
      file_path: String representing the path to the HTML file.
      xpath_selector: String representing xpath search pattern.

    Returns:
      A list of ElementTree.Elements matching the given XPath selector.
        Returns an empty list if there is no match.
    """
    with open(file_path) as f:
      file_contents = f.read().decode('ISO-8859-1').encode('UTF-8')
      root = ElementTree.fromstring(file_contents)
      return root.findall(xpath_selector)
