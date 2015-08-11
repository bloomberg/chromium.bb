/*
  qsufsort.h -- Suffix array generation.

  Copyright 2003 Colin Percival

  For the terms under which this work may be distributed, please see
  the adjoining file "LICENSE".

  ChangeLog:
  2005-05-05 - Use the modified header struct from bspatch.h; use 32-bit
               values throughout.
                 --Benjamin Smedberg <benjamin@smedbergs.us>
  2010-05-26 - Use a paged array for V and I. The address space may be too
               fragmented for these big arrays to be contiguous.
                 --Stephen Adams <sra@chromium.org>
  2015-08-03 - Extrat qsufsort to a separate file as template.
                 --Samuel Huang <huangs@chromium.org>
*/

#include <algorithm>
#include <cstring>

namespace courgette {
namespace qsuf {

// ------------------------------------------------------------------------
//
// The following code is taken verbatim from 'bsdiff.c'. Please keep all the
// code formatting and variable names.  The changes from the original are:
// (1) replacing tabs with spaces,
// (2) indentation,
// (3) using 'const',
// (4) changing the V and I parameters from int* to template <typename T>.
//
// The code appears to be a rewritten version of the suffix array algorithm
// presented in "Faster Suffix Sorting" by N. Jesper Larsson and Kunihiko
// Sadakane, special cased for bytes.

template <typename T>
static void
split(T I,T V,int start,int len,int h)
{
  int i,j,k,x,tmp,jj,kk;

  if(len<16) {
    for(k=start;k<start+len;k+=j) {
      j=1;x=V[I[k]+h];
      for(i=1;k+i<start+len;i++) {
        if(V[I[k+i]+h]<x) {
          x=V[I[k+i]+h];
          j=0;
        };
        if(V[I[k+i]+h]==x) {
          tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;
          j++;
        };
      };
      for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
      if(j==1) I[k]=-1;
    };
    return;
  };

  x=V[I[start+len/2]+h];
  jj=0;kk=0;
  for(i=start;i<start+len;i++) {
    if(V[I[i]+h]<x) jj++;
    if(V[I[i]+h]==x) kk++;
  };
  jj+=start;kk+=jj;

  i=start;j=0;k=0;
  while(i<jj) {
    if(V[I[i]+h]<x) {
      i++;
    } else if(V[I[i]+h]==x) {
      tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;
      j++;
    } else {
      tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;
      k++;
    };
  };

  while(jj+j<kk) {
    if(V[I[jj+j]+h]==x) {
      j++;
    } else {
      tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;
      k++;
    };
  };

  if(jj>start) split<T>(I,V,start,jj-start,h);

  for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
  if(jj==kk-1) I[jj]=-1;

  if(start+len>kk) split<T>(I,V,kk,start+len-kk,h);
}

template <class T>
static void
qsufsort(T I, T V,const unsigned char *old,int oldsize)
{
  int buckets[256];
  int i,h,len;

  for(i=0;i<256;i++) buckets[i]=0;
  for(i=0;i<oldsize;i++) buckets[old[i]]++;
  for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
  for(i=255;i>0;i--) buckets[i]=buckets[i-1];
  buckets[0]=0;

  for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
  I[0]=oldsize;
  for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
  V[oldsize]=0;
  for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
  I[0]=-1;

  for(h=1;I[0]!=-(oldsize+1);h+=h) {
    len=0;
    for(i=0;i<oldsize+1;) {
      if(I[i]<0) {
        len-=I[i];
        i-=I[i];
      } else {
        if(len) I[i-len]=-len;
        len=V[I[i]]+1-i;
        split<T>(I,V,i,len,h);
        i+=len;
        len=0;
      };
    };
    if(len) I[i-len]=-len;
  };

  for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

static int
matchlen(const unsigned char *old,int oldsize,const unsigned char *newbuf,int newsize)
{
  int i;

  for(i=0;(i<oldsize)&&(i<newsize);i++)
    if(old[i]!=newbuf[i]) break;

  return i;
}

template <class T>
static int
search(T I,const unsigned char *old,int oldsize,
       const unsigned char *newbuf,int newsize,int st,int en,int *pos)
{
  int x,y;

  if(en-st<2) {
    x=matchlen(old+I[st],oldsize-I[st],newbuf,newsize);
    y=matchlen(old+I[en],oldsize-I[en],newbuf,newsize);

    if(x>y) {
      *pos=I[st];
      return x;
    } else {
      *pos=I[en];
      return y;
    }
  }

  x=st+(en-st)/2;
  if(memcmp(old+I[x],newbuf,std::min(oldsize-I[x],newsize))<0) {
    return search<T>(I,old,oldsize,newbuf,newsize,x,en,pos);
  } else {
    return search<T>(I,old,oldsize,newbuf,newsize,st,x,pos);
  }
}

//  End of 'verbatim' code.
// ------------------------------------------------------------------------

}  // namespace qsuf
}  // namespace courgette
