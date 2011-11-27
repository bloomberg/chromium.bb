#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates profile dictionaries for Autofill.

Used to test autofill.AutofillTest.FormFillLatencyAfterSubmit.
Can be used as a stand alone script with -h to print out help text by running:
  python autofill_dataset_generator.py -h
"""

import codecs
import logging
from optparse import OptionParser
import os
import random
import re
import sys


class NullHandler(logging.Handler):
  def emit(self, record):
    pass


class DatasetGenerator(object):
  """Generates a dataset of dictionaries.

  The lists (such as address_construct, city_construct) define the way the
  corresponding field is generated. They accomplish this by specifying a
  list of function-args lists.
  """
  address_construct = [
    [ random.randint, 1, 10000],
    [ None, u'foobar'],
    [ random.choice, [ u'St', u'Ave', u'Ln', u'Ct', ]],
    [ random.choice, [ u'#1', u'#2', u'#3', ]],
  ]

  city_construct = [
    [ random.choice, [ u'San Jose', u'San Francisco', u'Sacramento',
                      u'Los Angeles', ]],
  ]

  state_construct = [
    [ None, u'CA']
  ]

  # These zip codes are now matched to the corresponding cities in
  # city_construct.
  zip_construct = [ u'95110', u'94109', u'94203', u'90120']

  logger = logging.getLogger(__name__)
  logger.addHandler(NullHandler())
  log_handlers = {'StreamHandler': None}

  def __init__(self, output_filename=None, logging_level=None):
    """Constructs dataset generator object.

    Creates 'fields' data member which is a list of pair (two values) lists.
    These pairs are comprised of a field key e.g. u'NAME_FIRST' and a
    generator method e.g. self.GenerateNameFirst which will generate the value.
    If we want the value to always be the same e.g. u'John' we can use this
    instead of a method. We can even use None keyword which will give
    a value of u''.

    'output_pattern' for one field would have been: "{u'NAME_FIRST': u'%s',}"
    which is ready to accept a value for the 'NAME_FIRST' field key once
    this value is generated.
    'output_pattern' is used in 'GenerateNextDict()' to generate the next
    dict line.

    Args:
      output_filename: specified filename of generated dataset to be saved.
        Default value is None and no saving takes place.
      logging_level: set verbosity levels, default is None.
    """
    if logging_level:
      if not self.log_handlers['StreamHandler']:
        console = logging.StreamHandler()
        console.setLevel(logging.INFO)
        self.log_handlers['StreamHandler'] = console
        self.logger.addHandler(console)
      self.logger.setLevel(logging_level)
    else:
      if self.log_handlers['StreamHandler']:
        self.logger.removeHandler(self.log_handlers['StreamHandler'])
        self.log_handlers['StreamHandler'] = None

    self.output_filename = output_filename

    self.dict_no = 0
    self.fields = [
      [u'NAME_FIRST', self.GenerateNameFirst],
      [u'NAME_MIDDLE', None],
      [u'NAME_LAST', None],
      [u'EMAIL_ADDRESS', self.GenerateEmail],
      [u'COMPANY_NAME', None],
      [u'ADDRESS_HOME_LINE1', self.GenerateAddress],
      [u'ADDRESS_HOME_LINE2', None],
      [u'ADDRESS_HOME_CITY', self.GenerateCity],
      [u'ADDRESS_HOME_STATE', self.GenerateState],
      [u'ADDRESS_HOME_ZIP', self.GenerateZip],
      [u'ADDRESS_HOME_COUNTRY', u'United States'],
      [u'PHONE_HOME_WHOLE_NUMBER', None],
    ]

    self.next_dict = {}
    # Using implicit line joining does not work well in this case as each line
    # has to be strings and not function calls that may return strings.
    self.output_pattern = u'{\'' + \
        u', '.join([u'u"%s" : u"%%s"' % key for key, method in self.fields]) + \
        u',}'

  def _GenerateField(self, field_construct):
    """Generates each field in each dictionary.

    Args:
      field_construct: it is a list of lists.
        The first value (index 0) of each containing list is a function or None.
        The remaining values are the args. If function is None then arg is just
        returned.

        Example 1: zip_construct = [[ None, u'95110']]. There is one
        containing list only and function here is None and arg is u'95110'.
        This just returns u'95110'.

        Example 2: address_construct = [ [ random.randint, 1, 10000],
        [ None, u'foobar'] ] This has two containing lists and it will return
        the result of:
        random.randint(1, 10000) + ' ' + u'foobar'
        which could be u'7832 foobar'
    """
    parts = []
    for function_and_args in field_construct:
      function = function_and_args[0]
      args = function_and_args[1:]
      if not function:
        function = lambda x: x
      parts.append(str(function(*args)))
    return (' ').join(parts)

  def GenerateAddress(self):
    """Uses _GenerateField() and address_construct to gen a random address.

    Returns:
      A random address.
    """
    return self._GenerateField(self.address_construct)

  def GenerateCity(self):
    """Uses _GenerateField() and city_construct to gen a random city.

    Returns:
      A random city.
    """
    return self._GenerateField(self.city_construct)

  def GenerateState(self):
    """Uses _GenerateField() and state_construct to generate a state.

    Returns:
      A state.
    """
    return self._GenerateField(self.state_construct)

  def GenerateZip(self):
    """Uses zip_construct and generated cities to return a matched zip code.

    Returns:
      A zip code matched to the corresponding city.
    """
    city_selected = self.next_dict['ADDRESS_HOME_CITY'][0]
    index = self.city_construct[0][1].index(city_selected)
    return self.zip_construct[index]

  def GenerateCountry(self):
    """Uses _GenerateField() and country_construct to generate a country.

    Returns:
      A country.
    """
    return self._GenerateField(self.country_construct)

  def GenerateNameFirst(self):
    """Generates a numerical first name.

    The name is the number of the current dict.
    i.e. u'1', u'2', u'3'

    Returns:
      A numerical first name.
    """
    return u'%s' % self.dict_no

  def GenerateEmail(self):
    """Generates an email that corresponds to the first name.

    i.e. u'1@example.com', u'2@example.com', u'3@example.com'

    Returns:
      An email address that corresponds to the first name.
    """
    return u'%s@example.com' % self.dict_no


  def GenerateNextDict(self):
    """Generates next dictionary of the dataset.

    Returns:
      The output dictionary.
    """
    self.dict_no += 1
    self.next_dict = {}
    for key, method_or_value in self.fields:
      if not method_or_value:
        self.next_dict[key] = ['']
      elif type(method_or_value) in [str, unicode]:
        self.next_dict[key] = ['%s' % method_or_value]
      else:
        self.next_dict[key] = [method_or_value()]
    return self.next_dict

  def GenerateDataset(self, num_of_dict_to_generate=10):
    """Generates a list of dictionaries.

    Args:
      num_of_dict_to_generate: The number of dictionaries to be generated.
      Default value is 10.

    Returns:
      The dictionary list.
    """
    random.seed(0)  # All randomly generated values are reproducible.
    if self.output_filename:
      output_file = codecs.open(
          self.output_filename, mode='wb', encoding='utf-8-sig')
    else:
      output_file = None
    try:
      list_of_dict = []
      if output_file:
        output_file.write('[')
        output_file.write(os.linesep)

      while self.dict_no < num_of_dict_to_generate:
        output_dict = self.GenerateNextDict()
        list_of_dict.append(output_dict)
        output_line = self.output_pattern % tuple(
            [output_dict[key] for key, method in self.fields])
        if output_file:
          output_file.write(output_line)
          output_file.write(os.linesep)
        self.logger.info(
            '%d: [%s]' % (self.dict_no, output_line.encode(sys.stdout.encoding,
                                                           'ignore')))

      if output_file:
        output_file.write(']')
        output_file.write(os.linesep)
      self.logger.info('%d dictionaries generated SUCCESSFULLY!', self.dict_no)
      self.logger.info('--- FINISHED ---')
      return list_of_dict
    finally:
      if output_file:
        output_file.close()


def main():
  parser = OptionParser()
  parser.add_option(
    '-o', '--output', dest='output_filename', default='',
    help='write output to FILE [optional]', metavar='FILE')
  parser.add_option(
    '-d', '--dict', type='int', dest='dict_no', metavar='DICT_NO', default=10,
    help='DICT_NO: number of dictionaries to be generated [default: %default]')
  parser.add_option(
    '-l', '--log_level', dest='log_level', default='debug',
    metavar='LOG_LEVEL',
    help='LOG_LEVEL: "debug", "info", "warning" or "error" [default: %default]')

  (options, args) = parser.parse_args()
  if args:
    parser.print_help()
    return 1
  options.log_level = options.log_level.lower()
  if options.log_level not in ['debug', 'info', 'warning', 'error']:
    parser.error('Wrong log_level argument.')
    parser.print_help()
  else:
    if options.log_level == 'debug':
      options.log_level = logging.DEBUG
    elif options.log_level == 'info':
      options.log_level = logging.INFO
    elif options.log_level == 'warning':
      options.log_level = logging.WARNING
    elif options.log_level == 'error':
      options.log_level = logging.ERROR

  gen = DatasetGenerator(options.output_filename, options.log_level)
  gen.GenerateDataset(options.dict_no)
  return 0


if __name__ == '__main__':
  sys.exit(main())
