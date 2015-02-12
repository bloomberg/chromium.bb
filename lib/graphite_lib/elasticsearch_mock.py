# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Mocks for ElasticSearch."""

from __future__ import print_function

from chromite.lib.graphite_lib import stats_es_mock


class Elasticsearch(stats_es_mock.mock_class_base):
  """Mock class for es_mock."""
  pass


class ElasticsearchException(Exception):
  """Mock class for elcasticsearch.ElasticsearchException."""
  pass
