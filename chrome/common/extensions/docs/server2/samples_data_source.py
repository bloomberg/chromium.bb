# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import re

DEFAULT_ICON_PATH = '/images/sample-default-icon.png'

class SamplesDataSource(object):
  """Constructs a list of samples and their respective files and api calls.
  """

  class Factory(object):
    """A factory to create SamplesDataSource instances bound to individual
    Requests.
    """
    def __init__(self, branch, file_system, cache_builder, samples_path):
      self._static_path = ((('/' + branch) if branch != 'local' else '') +
                           '/static')
      self._file_system = file_system
      self._cache = cache_builder.build(self._MakeSamplesList)
      self._samples_path = samples_path

    def Create(self, request):
      """Returns a new SamplesDataSource bound to |request|.
      """
      return SamplesDataSource(self._cache,
                               self._samples_path,
                               request)

    def _GetApiItems(self, js_file):
      return set(re.findall('(chrome\.[a-zA-Z0-9\.]+)', js_file))

    def _MakeApiLink(self, prefix, item):
      api, name = item.replace('chrome.', '').split('.', 1)
      return api + '.html#' + prefix + '-' + name

    def _GetDataFromManifest(self, path):
      manifest = self._file_system.ReadSingle(path + '/manifest.json')
      manifest_json = json.loads(manifest)
      l10n_data = {
        'name': manifest_json.get('name', ''),
        'description': manifest_json.get('description', ''),
        'icon': manifest_json.get('icons', {}).get('128', None),
        'default_locale': manifest_json.get('default_locale', None),
        'locales': {}
      }
      if not l10n_data['default_locale']:
        return l10n_data
      locales_path = path + '/_locales/'
      locales_dir = self._file_system.ReadSingle(locales_path)
      if locales_dir:
        locales_files = self._file_system.Read(
            [locales_path + f + 'messages.json' for f in locales_dir]).Get()
        locales_json = [(path, json.loads(contents))
                        for path, contents in locales_files.iteritems()]
        for path, json_ in locales_json:
          l10n_data['locales'][path[len(locales_path):].split('/')[0]] = json_
      return l10n_data

    def _MakeSamplesList(self, files):
      samples_list = []
      for filename in sorted(files):
        if filename.rsplit('/')[-1] != 'manifest.json':
          continue
        # This is a little hacky, but it makes a sample page.
        sample_path = filename.rsplit('/', 1)[-2]
        sample_files = [path for path in files
                        if path.startswith(sample_path + '/')]
        js_files = [path for path in sample_files if path.endswith('.js')]
        js_contents = self._file_system.Read(js_files).Get()
        api_items = set()
        for js in js_contents.values():
          api_items.update(self._GetApiItems(js))

        api_calls = []
        for item in api_items:
          if len(item.split('.')) < 3:
            continue
          if item.endswith('.addListener'):
            item = item.replace('.addListener', '')
            api_calls.append({
              'name': item,
              'link': self._MakeApiLink('event', item)
            })
          else:
            api_calls.append({
              'name': item,
              'link': self._MakeApiLink('method', item)
            })
        l10n_data = self._GetDataFromManifest(sample_path)
        sample_base_path = sample_path.split('/', 1)[1]
        if l10n_data['icon'] is None:
          icon_path = self._static_path + DEFAULT_ICON_PATH
        else:
          icon_path = sample_base_path + '/' + l10n_data['icon']
        l10n_data.update({
          'icon': icon_path,
          'path': sample_base_path,
          'files': [f.replace(sample_path + '/', '') for f in sample_files],
          'api_calls': api_calls
        })
        samples_list.append(l10n_data)

      return samples_list

  def __init__(self, cache, samples_path, request):
    self._cache = cache
    self._samples_path = samples_path
    self._request = request

  def _GetAcceptedLanguages(self):
    accept_language = self._request.headers.get('Accept-Language', None)
    if accept_language is None:
      return []
    return [lang_with_q.split(';')[0].strip()
            for lang_with_q in accept_language.split(',')]

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    samples_list = self._cache.GetFromFileListing(self._samples_path + '/')
    return_list = []
    for dict_ in samples_list:
      name = dict_['name']
      description = dict_['description']
      if name.startswith('__MSG_') or description.startswith('__MSG_'):
        try:
          # Copy the sample dict so we don't change the dict in the cache.
          sample_data = dict_.copy()
          name_key = name[len('__MSG_'):-len('__')]
          description_key = description[len('__MSG_'):-len('__')]
          locale = sample_data['default_locale']
          for lang in self._GetAcceptedLanguages():
            if lang in sample_data['locales']:
              locale = lang
              break
          locale_data = sample_data['locales'][locale]
          sample_data['name'] = locale_data[name_key]['message']
          sample_data['description'] = locale_data[description_key]['message']
        except Exception as e:
          logging.error(e)
          # Revert the sample to the original dict.
          sample_data = dict_
        return_list.append(sample_data)
      else:
        return_list.append(dict_)
    return return_list
