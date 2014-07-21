# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from api_categorizer import APICategorizer
from api_models import APIModels
from availability_finder import AvailabilityFinder
from features_bundle import FeaturesBundle
from future import Collect
from platform_util import GetPlatforms, PlatformToExtensionType
from reference_resolver import ReferenceResolver


class _PlatformData(object):
  def __init__(self):
    self.features_bundle = None
    self.api_models = None
    self.reference_resolver = None
    self.availability_finder = None
    self.api_categorizer = None


class PlatformBundle(object):
  '''Creates various objects for different platforms
  '''
  def __init__(self,
               branch_utility,
               compiled_fs_factory,
               host_fs_at_trunk,
               host_file_system_iterator,
               object_store_creator):
    self._branch_utility = branch_utility
    self._compiled_fs_factory = compiled_fs_factory
    self._host_fs_at_trunk = host_fs_at_trunk
    self._host_file_system_iterator = host_file_system_iterator
    self._object_store_creator = object_store_creator
    self._platform_data = dict((p, _PlatformData()) for p in GetPlatforms())

  def GetFeaturesBundle(self, platform):
    if self._platform_data[platform].features_bundle is None:
      self._platform_data[platform].features_bundle = FeaturesBundle(
          self._host_fs_at_trunk,
          self._compiled_fs_factory,
          self._object_store_creator,
          platform)
    return self._platform_data[platform].features_bundle

  def GetAPIModels(self, platform):
    if self._platform_data[platform].api_models is None:
      # TODO(danielj41): Filter APIModels data here rather than passing the
      # platform.
      self._platform_data[platform].api_models = APIModels(
          self.GetFeaturesBundle(platform),
          self._compiled_fs_factory,
          self._host_fs_at_trunk,
          self._object_store_creator,
          platform)
    return self._platform_data[platform].api_models

  def GetReferenceResolver(self, platform):
    if self._platform_data[platform].reference_resolver is None:
      self._platform_data[platform].reference_resolver = ReferenceResolver(
          self.GetAPIModels(platform),
          self._object_store_creator.Create(ReferenceResolver,
                                            category=platform))
    return self._platform_data[platform].reference_resolver

  def GetAvailabilityFinder(self, platform):
    if self._platform_data[platform].availability_finder is None:
      self._platform_data[platform].availability_finder = AvailabilityFinder(
          self._branch_utility,
          self._compiled_fs_factory,
          self._host_file_system_iterator,
          self._host_fs_at_trunk,
          self._object_store_creator,
          platform)
    return self._platform_data[platform].availability_finder

  def GetAPICategorizer(self, platform):
    if self._platform_data[platform].api_categorizer is None:
      self._platform_data[platform].api_categorizer = APICategorizer(
          self._host_fs_at_trunk,
          self._compiled_fs_factory,
          platform)
    return self._platform_data[platform].api_categorizer

  def Cron(self):
    return Collect([self.GetAPIModels(platform).Cron()
                    for platform in self._platform_data])

  def GetIdentity(self):
    return self._host_fs_at_trunk.GetIdentity()
