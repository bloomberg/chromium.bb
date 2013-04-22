#-------------------------------------------------------------------------------
# elftools: elf/dynamic.py
#
# ELF Dynamic Tags
#
# Mike Frysinger (vapier@gentoo.org)
# This code is in the public domain
#-------------------------------------------------------------------------------
import itertools

from .sections import Section
from .segments import Segment
from ..common.utils import struct_parse, parse_cstring_from_stream

from .enums import ENUM_D_TAG


class _DynamicStringTable(object):
    """ Bare string table based on values found via ELF dynamic tags and
        loadable segments only.  Good enough for get_string() only.
    """
    def __init__(self, stream, table_offset):
        self._stream = stream
        self._table_offset = table_offset

    def get_string(self, offset):
        """ Get the string stored at the given offset in this string table.
        """
        return parse_cstring_from_stream(self._stream,
                                         self._table_offset + offset)


class DynamicTag(object):
    """ Dynamic Tag object - representing a single dynamic tag entry from a
        dynamic section.

        Allows dictionary-like access to the dynamic structure. For special
        tags (those listed in the _HANDLED_TAGS set below), creates additional
        attributes for convenience. For example, .soname will contain the actual
        value of DT_SONAME (fetched from the dynamic symbol table).
    """
    _HANDLED_TAGS = frozenset(
        ['DT_NEEDED', 'DT_RPATH', 'DT_RUNPATH', 'DT_SONAME'])

    def __init__(self, entry, dynstr):
        self.entry = entry
        if entry.d_tag in self._HANDLED_TAGS and dynstr:
            setattr(self, entry.d_tag[3:].lower(),
                    dynstr.get_string(self.entry.d_val))

    def __getitem__(self, name):
        """ Implement dict-like access to entries
        """
        return self.entry[name]

    def __repr__(self):
        return '<DynamicTag (%s): %r>' % (self.entry.d_tag, self.entry)

    def __str__(self):
        if self.entry.d_tag in self._HANDLED_TAGS:
            s = '"%s"' % getattr(self, self.entry.d_tag[3:].lower())
        else:
            s = '%#x' % self.entry.d_ptr
        return '<DynamicTag (%s) %s>' % (self.entry.d_tag, s)


class Dynamic(object):
    """ Shared functionality between dynamic sections and segments.
    """
    def __init__(self, stream, elffile, position):
        self._stream = stream
        self._elffile = elffile
        self._elfstructs = elffile.structs
        self._num_tags = -1
        self._offset = position
        self._tagsize = self._elfstructs.Elf_Dyn.sizeof()
        self.__string_table = None

    @property
    def _string_table(self):
        """ Return a string table for looking up dynamic tag related strings.

        This won't be a "full" string table object, but will at least support
        the get_string() function.
        """
        if self.__string_table:
            return self.__string_table

        # If the ELF has stripped its section table (which is unusual, but
        # perfectly valid), we need to use the dynamic tags to locate the
        # dynamic string table.
        strtab = None
        for tag in self._iter_tags(type='DT_STRTAB'):
            strtab = tag['d_val']
            break
        # If we found a dynamic string table, locate the offset in the file
        # by using the program headers.
        if strtab:
            for segment in self._elffile.iter_segments():
                if (strtab >= segment['p_vaddr'] and
                    strtab < segment['p_vaddr'] + segment['p_filesz']):
                    self.__string_table = _DynamicStringTable(
                        self._stream,
                        segment['p_offset'] + (strtab - segment['p_vaddr']))
                    return self.__string_table

        # That didn't work for some reason.  Let's use the section header
        # even though this ELF is super weird.
        self.__string_table = self._elffile.get_section_by_name(b'.dynstr')

        return self.__string_table

    def _iter_tags(self, type=None):
        """ Yield all raw tags (limit to |type| if specified)
        """
        for n in itertools.count():
            tag = self._get_tag(n)
            if type is None or tag['d_tag'] == type:
                yield tag
            if tag['d_tag'] == 'DT_NULL':
                break

    def iter_tags(self, type=None):
        """ Yield all tags (limit to |type| if specified)
        """
        for tag in self._iter_tags(type=type):
            yield DynamicTag(tag, self._string_table)

    def _get_tag(self, n):
        """ Get the raw tag at index #n from the file
        """
        offset = self._offset + n * self._tagsize
        return struct_parse(
            self._elfstructs.Elf_Dyn,
            self._stream,
            stream_pos=offset)

    def get_tag(self, n):
        """ Get the tag at index #n from the file (DynamicTag object)
        """
        return DynamicTag(self._get_tag(n), self._string_table)

    def num_tags(self):
        """ Number of dynamic tags in the file
        """
        if self._num_tags != -1:
            return self._num_tags

        for n in itertools.count():
            tag = self.get_tag(n)
            if tag.entry.d_tag == 'DT_NULL':
                self._num_tags = n + 1
                return self._num_tags


class DynamicSection(Section, Dynamic):
    """ ELF dynamic table section.  Knows how to process the list of tags.
    """
    def __init__(self, header, name, stream, elffile):
        Section.__init__(self, header, name, stream)
        Dynamic.__init__(self, stream, elffile, self['sh_offset'])


class DynamicSegment(Segment, Dynamic):
    """ ELF dynamic table segment.  Knows how to process the list of tags.
    """
    def __init__(self, header, stream, elffile):
        Segment.__init__(self, header, stream)
        Dynamic.__init__(self, stream, elffile, self['p_offset'])
