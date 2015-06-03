# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module allows for easy access to common system information.

In order to reduce stress due to excessive checking of system information,
the records collected by the classes in this file are stored with a period
of validity. If a record is requested, and there is an existing record
of the same type, and it is still valid, it is returned. If there is no
existing record, or it has gone stale, then the system information is
actually collected.

Each class uses a few attributes to regulate when they should do
a collection, they are:
  update_sec: An integer that is the period of validity for a record.
  update_times: A dictionary that maps a resource name to an epoch time.
  resources: A dictionary that maps a resource name to the actual record.

On retrieving a new record, the time at which it is collected is stored
in update_times with the record name as key, and the record itself is
stored in resources. Every subsequent collection returns what is stored
in the resources dict until the record goes stale.
"""

from __future__ import print_function

import collections
import functools
import itertools
import os
import time


SYSTEMFILE_PROC_MOUNTS = '/proc/mounts'
SYSTEMFILE_PROC_MEMINFO = '/proc/meminfo'
SYSTEMFILE_PROC_FILESYSTEMS = '/proc/filesystems'
SYSTEMFILE_PROC_STAT = '/proc/stat'


RESOURCENAME_MEMORY = 'memory'
RESOURCENAME_DISKPARTITIONS = 'diskpartitions'
RESOURCENAME_DISKUSAGE = 'diskusage'
RESOURCENAME_CPUPREVTIMES = 'cpuprevtimes'
RESOURCENAME_CPUTIMES = 'cputimes'
RESOURCENAME_CPULOADS = 'cpuloads'

RESOURCE_MEMORY = collections.namedtuple('memory', ['total', 'available',
                                                    'percent_used'])
RESOURCE_DISKPARTITION = collections.namedtuple('diskpartition',
                                                ['total', 'used', 'free'])
RESOURCE_DISKUSAGE = collections.namedtuple('diskusage', ['total', 'used',
                                                          'free',
                                                          'percent_used'])
RESOURCE_CPUTIME = collections.namedtuple('cputime', ['cpu', 'total', 'idle',
                                                      'nonidle'])
RESOURCE_CPULOAD = collections.namedtuple('cpuload', ['cpu', 'load'])


CPU_ATTRS = ('cpu', 'user', 'nice', 'system', 'idle', 'iowait', 'irq',
             'softirq', 'steal', 'guest', 'guest_nice')
CPU_IDLE_ATTRS = frozenset(['idle', 'iowait'])
CPU_NONIDLE_ATTRS = frozenset(['user', 'nice', 'system', 'irq', 'softirq'])


def CheckStorage(resource_basename):
  """Decorate information functions to retrieve stored records if valid.

  Args:
    resource_basename: The data value basename we are checking our
      local storage for.

  Returns:
    The real function decorator.
  """

  def func_deco(func):
    """Return stored record if valid, else run function and update storage.

    Args:
      func: The collection function we are executing.

    Returns:
      The function wrapper.
    """

    @functools.wraps(func)
    def wrapper(self, *args, **kwargs):
      """Function wrapper.

      Args:
        args: Positional arguments that will be appended to dataname when
          searching the local storage.

      Returns:
        The stored record or the new record as a result of running the
        collection function.
      """
      dataname = resource_basename
      if args:
        dataname = '%s:%s' % (dataname, args[0])

      if not self.NeedToUpdate(dataname):
        return self.resources.get(dataname)

      datavalue = func(self, *args, **kwargs)
      self.Update(dataname, datavalue)

      return datavalue

    return wrapper

  return func_deco


class SystemInfoStorage(object):
  """Store and access system information."""

  def __init__(self, update_sec=30):
    self.update_sec = update_sec
    self.update_times = {}
    self.resources = {}

  def Update(self, resource_name, data):
    """Update local storage and collection times of the data.

    Args:
      resource_name: The key used for local storage and update times.
      data: The data to store that is keyed by resource_name.
    """
    self.update_times[resource_name] = time.time()
    self.resources[resource_name] = data

  def NeedToUpdate(self, resource_name):
    """Check if the record keyed by resource_name needs to be (re-)collected.

    Args:
      resource_name: A string representing some system value.

    Returns:
      A boolean. If True, the data must be collected. If False, the data
      be retrieved from the self.resources dict with key resource_name.
    """
    if resource_name not in self.resources:
      return True

    if resource_name not in self.update_times:
      return True

    return time.time() > self.update_sec + self.update_times.get(resource_name)


class Memory(SystemInfoStorage):
  """Access memory information."""

  def __init__(self, update_sec=30):
    super(Memory, self).__init__(update_sec=update_sec)

  @CheckStorage(RESOURCENAME_MEMORY)
  def MemoryUsage(self):
    """Collect memory information from /proc/meminfo.

    Returns:
      A named tuple with the following fields:
        total: Corresponds to MemTotal of /proc/meminfo.
        available: Corresponds to (MemFree+Buffers+Cached) of /proc/meminfo.
        percent_used: The percentage of memory that is used based on
          total and available.
    """
    # See MOCK_PROC_MEMINFO in the unittest file for this module
    # to see an example of the file this function is reading from.
    memtotal, memfree, buffers, cached = (0, 0, 0, 0)
    with open(SYSTEMFILE_PROC_MEMINFO, 'rb') as f:
      for line in f:
        if line.startswith('MemTotal'):
          memtotal = int(line.split()[1]) * 1024
        if line.startswith('MemFree'):
          memfree = int(line.split()[1]) * 1024
        if line.startswith('Buffers'):
          buffers = int(line.split()[1]) * 1024
        if line.startswith('Cached'):
          cached = int(line.split()[1]) * 1024

    available = memfree + buffers + cached
    percent_used = float(memtotal - available) / memtotal * 100

    memory = RESOURCE_MEMORY(memtotal, available, percent_used)

    return memory


class Disk(SystemInfoStorage):
  """Access disk information."""

  def __init__(self, update_sec=30):
    super(Disk, self).__init__(update_sec=update_sec)

  @CheckStorage(RESOURCENAME_DISKPARTITIONS)
  def DiskPartitions(self):
    """Collect basic information about disk partitions.

    Returns:
      A list of named tuples. Each named tuple has the following fields:
        mountname: The name of the partition.
        mountpoint: The mount point of the partition.
        filesystem: The file system in use on the partition.
    """
    # Read /proc/mounts for mounted filesystems.
    # See MOCK_PROC_MOUNTS in the unittest file for this module
    # to see an example of the file this function is reading from.
    mounts = []
    with open(SYSTEMFILE_PROC_MOUNTS, 'rb') as f:
      for line in f:
        iterline = iter(line.split())
        try:
          mounts.append([next(iterline), next(iterline), next(iterline)])
        except StopIteration:
          pass

    # Read /proc/filesystems for a list of physical filesystems
    # See MOCK_PROC_FILESYSTEMS in the unittest file for this module
    # to see an example of the file this function is reading from.
    physmounts = []
    with open(SYSTEMFILE_PROC_FILESYSTEMS, 'rb') as f:
      for line in f:
        if not line.startswith('nodev'):
          physmounts.append(line.strip())

    # From these two sources, create a list of partitions
    diskpartitions = []
    for mountname, mountpoint, filesystem in mounts:
      if filesystem not in physmounts:
        continue
      diskpartition = RESOURCE_DISKPARTITION(mountname, mountpoint, filesystem)
      diskpartitions.append(diskpartition)

    return diskpartitions

  @CheckStorage(RESOURCENAME_DISKUSAGE)
  def DiskUsage(self, partition):
    """Collects usage information for the specified partition.

    Args:
      partition: The partition for which to check usage. This is the
        same as the 'mountpoint' attribute given in the return value
        of DiskPartitions.

    Returns:
      A named tuple with the following fields:
        total: The total space on the partition.
        used: The total amount of used space on the parition.
        free: The total amount of unused space on the partition.
        percent_used: The percentage of the partition that is used
          based on total and used.
    """
    # Collect the partition information
    vfsdata = os.statvfs(partition)
    total = vfsdata.f_frsize * vfsdata.f_blocks
    free = vfsdata.f_bsize * vfsdata.f_bfree
    used = total - free
    percent_used = float(used) / total * 100

    diskusage = RESOURCE_DISKUSAGE(total, used, free, percent_used)

    return diskusage


class Cpu(SystemInfoStorage):
  """Access CPU information."""

  def __init__(self, update_sec=2):
    super(Cpu, self).__init__(update_sec=update_sec)

    # CpuLoad depends on having two CpuTime collections at different
    # points in time. One issue, is that the first call to CpuLoad,
    # without prior calls to CpuTime will return a trivial value, that
    # is, all cpus will be reported to have zero load. We solve this
    # by doing an initial CpuTime collection here.
    self.CpuTime()
    self.update_times.pop(RESOURCENAME_CPUTIMES)
    self.Update(RESOURCENAME_CPUPREVTIMES,
                self.resources.pop(RESOURCENAME_CPUTIMES))

  @CheckStorage(RESOURCENAME_CPUTIMES)
  def CpuTime(self):
    """Collect information on CPU time.

    Returns:
      A list of named tuples. Each named tuple has the following fields:
        cpu: An identifier for the CPU.
        total: The total CPU time in the measurement.
        idle: The total time spent in an idle state.
        nonidle: The total time spent not in an idle state.
    """
    # Collect CPU time information from /proc/stat
    cputimes = []

    # See MOCK_PROC_STAT in the unittest file for this module
    # to see an example of the file this function is reading from.
    with open(SYSTEMFILE_PROC_STAT, 'rb') as f:
      for line in f:
        if not line.startswith('cpu'):
          continue
        cpudesc = dict(itertools.izip(CPU_ATTRS, line.split()))
        idle, nonidle = (0, 0)
        for attr, value in cpudesc.iteritems():
          if attr in CPU_IDLE_ATTRS:
            idle += int(value)
          if attr in CPU_NONIDLE_ATTRS:
            nonidle += int(value)
        total = idle + nonidle
        cputimes.append(RESOURCE_CPUTIME(cpudesc.get('cpu'),
                                         total, idle, nonidle))

    # Store the previous cpu times if we have a 'current' measurement
    # that is about to be replaced. This is very helpful for calculating
    # load estimates over the update interval.
    if RESOURCENAME_CPUTIMES in self.resources:
      self.Update(RESOURCENAME_CPUPREVTIMES,
                  self.resources.get(RESOURCENAME_CPUTIMES))

    return cputimes

  @CheckStorage(RESOURCENAME_CPULOADS)
  def CpuLoad(self):
    """Estimate the CPU load.

    Returns:
      A list of named tuples. Each name tuple has the following fields:
        cpu: An identifier for the CPU.
        load: A number representing the load/usage ranging between 0 and 1.
    """
    prevcputimes = self.resources.get(RESOURCENAME_CPUPREVTIMES)
    cputimes = self.CpuTime()
    cpuloads = []
    for prevtime, curtime in itertools.izip(prevcputimes, cputimes):
      ct = curtime.total
      ci = curtime.idle
      pt = prevtime.total
      pi = prevtime.idle

      # Cpu load is estimated using a difference between cpu timing collections
      # taken at different points in time. To estimate how much time in that
      # interval was spent in a non-idle state, we calculate the percent change
      # of the non-idle time using the relative differences in total and idle
      # time between the two collections.
      cpu = curtime.cpu
      load = float((ct-pt)-(ci-pi))/(ct-pt) if (ct-pt) != 0 else 0
      cpuloads.append(RESOURCE_CPULOAD(cpu, load))

    return cpuloads
