# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import traceback

from data_source import DataSource
from extensions_paths import EXAMPLES
from future import All, Future
from platform_util import GetPlatforms


def _GetSampleId(sample_name):
  return sample_name.lower().replace(' ', '-')


def GetAcceptedLanguages(request):
  if request is None:
    return []
  accept_language = request.headers.get('Accept-Language', None)
  if accept_language is None:
    return []
  return [lang_with_q.split(';')[0].strip()
          for lang_with_q in accept_language.split(',')]


def CreateSamplesView(samples_list, request):
  return_list = []
  for dict_ in samples_list:
    name = dict_['name']
    description = dict_['description']
    if description is None:
      description = ''
    if name.startswith('__MSG_') or description.startswith('__MSG_'):
      try:
        # Copy the sample dict so we don't change the dict in the cache.
        sample_data = dict_.copy()
        name_key = name[len('__MSG_'):-len('__')]
        description_key = description[len('__MSG_'):-len('__')]
        locale = sample_data['default_locale']
        for lang in GetAcceptedLanguages(request):
          if lang in sample_data['locales']:
            locale = lang
            break
        locale_data = sample_data['locales'][locale]
        sample_data['name'] = locale_data[name_key]['message']
        sample_data['description'] = locale_data[description_key]['message']
        sample_data['id'] = _GetSampleId(sample_data['name'])
      except Exception:
        logging.error(traceback.format_exc())
        # Revert the sample to the original dict.
        sample_data = dict_
      return_list.append(sample_data)
    else:
      dict_['id'] = _GetSampleId(name)
      return_list.append(dict_)
  return return_list


class SamplesDataSource(DataSource):
  '''Constructs a list of samples and their respective files and api calls.
  '''
  def __init__(self, server_instance, request):
    self._platform_bundle = server_instance.platform_bundle
    self._request = request

  def _GetImpl(self, platform):
    cache = self._platform_bundle.GetSamplesModel(platform).GetCache()
    create_view = lambda samp_list: CreateSamplesView(samp_list, self._request)
    return cache.GetFromFileListing('' if platform == 'apps'
                                       else EXAMPLES).Then(create_view)

  def get(self, platform):
    return self._GetImpl(platform).Get()

  def Cron(self):
    return All([self._GetImpl(platform) for platform in GetPlatforms()])
