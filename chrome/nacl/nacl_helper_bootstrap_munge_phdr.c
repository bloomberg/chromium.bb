/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is a trivial program to edit an ELF file in place, making
 * one crucial modification to a program header.  It's invoked:
 *      bootstrap_phdr_hacker FILENAME SEGMENT_NUMBER
 * where SEGMENT_NUMBER is the zero-origin index of the program header
 * we'll touch.  This is a PT_LOAD with p_filesz of zero.  We change its
 * p_filesz to match its p_memsz value.
 */

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 3)
    error(1, 0, "Usage: %s FILENAME SEGMENT_NUMBER", argv[0]);

  const char *const file = argv[1];
  const int segment = atoi(argv[2]);

  int fd = open(file, O_RDWR);
  if (fd < 0)
    error(2, errno, "Cannot open %s for read/write", file);

  if (elf_version(EV_CURRENT) == EV_NONE)
    error(2, 0, "elf_version: %s", elf_errmsg(-1));

  Elf *elf = elf_begin(fd, ELF_C_RDWR, NULL);
  if (elf == NULL)
    error(2, 0, "elf_begin: %s", elf_errmsg(-1));

  if (elf_flagelf(elf, ELF_C_SET, ELF_F_LAYOUT) == 0)
    error(2, 0, "elf_flagelf: %s", elf_errmsg(-1));

  GElf_Phdr phdr;
  GElf_Phdr *ph = gelf_getphdr(elf, segment, &phdr);
  if (ph == NULL)
    error(2, 0, "gelf_getphdr: %s", elf_errmsg(-1));

  if (ph->p_type != PT_LOAD)
    error(3, 0, "Program header %d is %u, not PT_LOAD",
          segment, (unsigned int) ph->p_type);
  if (ph->p_filesz != 0)
    error(3, 0, "Program header %d has nonzero p_filesz", segment);

  ph->p_filesz = ph->p_memsz;
  if (gelf_update_phdr(elf, segment, ph) == 0)
    error(2, 0, "gelf_update_phdr: %s", elf_errmsg(-1));

  if (elf_flagphdr(elf, ELF_C_SET, ELF_F_DIRTY) == 0)
    error(2, 0, "elf_flagphdr: %s", elf_errmsg(-1));

  if (elf_update(elf, ELF_C_WRITE) < 0)
    error(2, 0, "elf_update: %s", elf_errmsg(-1));

  close(fd);

  return 0;
}
