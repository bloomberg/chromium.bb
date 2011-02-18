#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Converts profile datasets to dictionary list for Autofill profiles.

Used for test autofill.AutoFillTest.testMergeDuplicateProfilesInAutofill.
"""

import codecs
import logging
import os
import re
import sys


class DatasetConverter(object):
  _fields = [
    u'NAME_FIRST',
    u'NAME_MIDDLE',
    u'NAME_LAST',
    u'EMAIL_ADDRESS',
    u'COMPANY_NAME',
    u'ADDRESS_HOME_LINE1',
    u'ADDRESS_HOME_LINE2',
    u'ADDRESS_HOME_CITY',
    u'ADDRESS_HOME_STATE',
    u'ADDRESS_HOME_ZIP',
    u'ADDRESS_HOME_COUNTRY',
    u'PHONE_HOME_WHOLE_NUMBER',
    u'PHONE_FAX_WHOLE_NUMBER',
  ]
  _record_length = len(_fields)
  _output_pattern = u'{'
  for key in _fields:
    _output_pattern += u"u'%s': u'%%s', " % key
  _output_pattern = _output_pattern[:-1] + '},'
  _re_single_quote = re.compile("'", re.UNICODE)
  _logger = logging.getLogger(__name__)

  def __init__(self, input_filename, output_filename=None,
               logging_level=logging.ERROR):
    """Constructs a dataset converter object.

    Full input pattern:
      '(?P<NAME_FIRST>.*?)\|(?P<MIDDLE_NAME>.*?)\|(?P<NAME_LAST>.*?)\|
      (?P<EMAIL_ADDRESS>.*?)\|(?P<COMPANY_NAME>.*?)\|(?P<ADDRESS_HOME_LINE1>.*?)
      \|(?P<ADDRESS_HOME_LINE2>.*?)\|(?P<ADDRESS_HOME_CITY>.*?)\|
      (?P<ADDRESS_HOME_STATE>.*?)\|(?P<ADDRESS_HOME_ZIP>.*?)\|
      (?P<ADDRESS_HOME_COUNTRY>.*?)\|
      (?P<PHONE_HOME_WHOLE_NUMBER>.*?)\|(?P<PHONE_FAX_WHOLE_NUMBER>.*?)$'

    Full ouput pattern:
      "{u'NAME_FIRST': u'%s', u'NAME_MIDDLE': u'%s', u'NAME_LAST': u'%s',
      u'EMAIL_ADDRESS': u'%s', u'COMPANY_NAME': u'%s', u'ADDRESS_HOME_LINE1':
      u'%s', u'ADDRESS_HOME_LINE2': u'%s', u'ADDRESS_HOME_CITY': u'%s',
      u'ADDRESS_HOME_STATE': u'%s', u'ADDRESS_HOME_ZIP': u'%s',
      u'ADDRESS_HOME_COUNTRY': u'%s', u'PHONE_HOME_WHOLE_NUMBER': u'%s',
      u'PHONE_FAX_WHOLE_NUMBER': u'%s',},"

    Args:
      input_filename: name and path of the input dataset.
      output_filename: name and path of the converted file, default is none.
      logging_level: set verbosity levels, default is ERROR.

    Raises:
      IOError: error if input file does not exist.
    """
    console = logging.StreamHandler()
    console.setLevel(logging_level)
    self._logger.addHandler(console)

    self._input_filename = os.path.join(os.path.dirname(sys.argv[0]),
                                        input_filename)
    if not os.path.isfile(self._input_filename):
      msg = 'File "%s" does not exist' % self._input_filename
      self._logger.error(msg)
      raise IOError(msg)
    self._output_filename = output_filename

  def _CreateDictionaryFromRecord(self, record):
    """Constructs and returns a dictionary from a record in the dataset file.

    Escapes single quotation first and uses split('|') to separate values.
    The method assumes a valid record always contains at least one "|"
    character.
    Example:
      Take an argument as a string u'John|Doe|Mountain View'
      and returns a dictionary
      {
      u'NAME_FIRST': u'John',
      u'NAME_LAST': u'Doe',
      u'ADDRESS_HOME_CITY': u'Mountain View',
      }

    Args:
      record: row of record from the dataset file.

    Returns:
      None if the current record line is invalid or a dictionary representing a
      single record from the dataset file.
    """
    # Ignore irrelevant record lines that do not contain '|'.
    if not '|' in record:
      return
    # Escaping single quote: "'" -> "\'"
    record = self._re_single_quote.sub(r"\'", record)
    record_list = record.split('|')
    if record_list:
      # Check for case when a record may have more or less fields than expected.
      if len(record_list) != self._record_length:
        self._logger.warning(
            'A "|" separated line has %d fields instead of %d: %s' % (
                len(record_list), self._record_length, record))
        return
      out_record = {}
      for i, key in enumerate(self._fields):
        out_record[key] = record_list[i]
      return out_record

  def Convert(self):
    """Function to convert input data into the desired output format.

    Returns:
      List that holds all the dictionaries.
    """
    with open(self._input_filename) as input_file:
      if self._output_filename:
        output_file = codecs.open(self._output_filename, mode='wb',
                                  encoding='utf-8-sig')
      else:
        output_file = None
      try:
        list_of_dict = []
        i = 0
        if output_file:
          output_file.write('[')
          output_file.write(os.linesep)
        for line in input_file.readlines():
          line = line.strip()
          if not line:
            continue
          line = unicode(line, 'UTF-8')
          output_record = self._CreateDictionaryFromRecord(line)
          if output_record:
            i += 1
            list_of_dict.append(output_record)
            output_line = self._output_pattern % tuple(
                [output_record[key] for key in self._fields])
            if output_file:
              output_file.write(output_line)
              output_file.write(os.linesep)
            self._logger.info('%d: %s' % (i, line.encode(sys.stdout.encoding,
                                                         'ignore')))
            self._logger.info('\tconverted to: %s' %
                              output_line.encode(sys.stdout.encoding, 'ignore'))
        if output_file:
          output_file.write(']')
          output_file.write(os.linesep)
        self._logger.info('%d lines converted SUCCESSFULLY!' % i)
        self._logger.info('--- FINISHED ---')
        return list_of_dict
      finally:
        if output_file:
          output_file.close()


def main():
  c = DatasetConverter(r'../data/autofill/dataset.txt',
                       r'../data/autofill/dataset_duplicate-profiles.txt',
                       logging.INFO)
  c.Convert()

if __name__ == '__main__':
  main()
