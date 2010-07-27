#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto


class TranslateTest(pyauto.PyUITest):
  """Tests that translate works correctly"""

  spanish_google = 'http://www.google.com/webhp?hl=es'
  spanish = 'es'
  after_translate = 'AFTER_TRANSLATE'
  before_translate = 'BEFORE_TRANSLATE'
  translating = 'TRANSLATING'
  translation_error = 'TRANSLATION_ERROR'

  def Debug(self):
    """ Test method for experimentation. """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Hit <enter> to dump translate info.. ')
      pp.pprint(self.GetTranslateInfo())

  def _NavigateAndWaitForBar(self, url):
    self.NavigateToURL(url)
    self.WaitForInfobarCount(1)

  def _ClickTranslateUntilSuccess(self):
    """Since the translate can fail due to server error, continue trying until
       it is successful or until it has tried too many times."""
    max_tries = 10
    curr_try = 0
    while curr_try < max_tries and not self.ClickTranslateBarTranslate():
      curr_try = curr_try + 1
    if curr_try == 10:
      self.fail('Translation failed more than %d times.' % max_tries)

  def testTranslate(self):
    """Tests that a page was translated if the user chooses to translate."""
    self._NavigateAndWaitForBar(self.spanish_google)
    self._ClickTranslateUntilSuccess()
    translate_info = self.GetTranslateInfo()
    self.assertEqual(self.spanish, translate_info['original_language'])
    self.assertTrue(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self.assertEquals(self.after_translate,
                      translate_info['translate_bar']['bar_state'])

  def testNoTranslate(self):
    """Tests that a page isn't translated if the user declines translate."""
    self._NavigateAndWaitForBar(self.spanish_google)
    self.PerformActionOnInfobar('dismiss', 0)
    translate_info = self.GetTranslateInfo()
    self.assertEqual(self.spanish, translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    # If the user goes to the site again, the infobar should drop down but
    # the page should not be automatically translated.
    self._NavigateAndWaitForBar(self.spanish_google)
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self.assertEquals(self.before_translate,
                      translate_info['translate_bar']['bar_state'])

  def testNeverTranslateLanguage(self):
    """Tests that blacklisting a language for translate works."""
    self._NavigateAndWaitForBar(self.spanish_google)
    self.SelectTranslateOption('never_translate_language')
    translate_info = self.GetTranslateInfo()
    self.assertEqual(self.spanish, translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertFalse(translate_info['can_translate_page'])
    spanish_wikipedia = 'http://es.wikipedia.org/wiki/Wikipedia:Portada'
    self.NavigateToURL(spanish_wikipedia)
    translate_info = self.GetTranslateInfo()
    self.assertEqual(self.spanish, translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertFalse(translate_info['can_translate_page'])

  def testAlwaysTranslateLanguage(self):
    """Tests that the always translate a language option works."""
    self._NavigateAndWaitForBar(self.spanish_google)
    self.SelectTranslateOption('toggle_always_translate')
    self._ClickTranslateUntilSuccess()
    translate_info = self.GetTranslateInfo()
    self.assertEquals(self.spanish, translate_info['original_language'])
    self.assertTrue(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self.assertEquals(self.after_translate,
                      translate_info['translate_bar']['bar_state'])
    # Go to another spanish site and verify that it is translated.
    # Note that the 'This page has been translated..." bar will show up.
    self._NavigateAndWaitForBar(
        'http://es.wikipedia.org/wiki/Wikipedia:Portada')
    translate_info = self.GetTranslateInfo()
    self.assertTrue('translate_bar' in translate_info)
    curr_bar_state = translate_info['translate_bar']['bar_state']
    # We don't care whether the translation has finished, just that it is
    # trying to translate.
    self.assertTrue(curr_bar_state is self.after_translate or
                    self.translating or self.translation_error,
                    'Bar state was %s.' % curr_bar_state)

  def testNeverTranslateSite(self):
    """Tests that blacklisting a site for translate works."""
    self._NavigateAndWaitForBar(self.spanish_google)
    self.SelectTranslateOption('never_translate_site')
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertFalse(translate_info['can_translate_page'])
    french_google = 'http://www.google.com/webhp?hl=fr'
    # Go to another page in the same site and verify that the page can't be
    # translated even though it's in a different language.
    self.NavigateToURL(french_google)
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertFalse(translate_info['can_translate_page'])

  def testRevert(self):
    """Tests that reverting a site to its original language works."""
    self._NavigateAndWaitForBar(self.spanish_google)
    self._ClickTranslateUntilSuccess()
    self.RevertPageTranslation()
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])


if __name__ == '__main__':
  pyauto_functional.Main()
