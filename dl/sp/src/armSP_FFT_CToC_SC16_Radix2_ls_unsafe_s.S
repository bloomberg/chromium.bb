;//
;// 
;// File Name:  armSP_FFT_CToC_SC16_Radix2_ls_unsafe_s.s
;// OpenMAX DL: v1.0.2
;// Last Modified Revision:   6741
;// Last Modified Date:       Wed, 18 Jul 2007
;// 
;// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
;// 
;// 
;//
;// Description:
;// Compute a Radix 2 FFT stage for a N point complex signal
;// 
;// 

        
;// Include standard headers

        INCLUDE omxtypes_s.h
        INCLUDE armCOMM_s.h
        
        M_VARIANTS CortexA8
        
;// Import symbols required from other files
;// (For example tables)
    
        
        
        
;// Set debugging level        
;//DEBUG_ON    SETL {TRUE}


;// Guarding implementation by the processor name
    
    




            
;// Guarding implementation by the processor name
    
    IF  CortexA8 
    
;//Input Registers

pSrc            RN  0
pDst            RN  2
pTwiddle        RN  1
subFFTNum       RN  6
subFFTSize      RN  7


;//Output Registers


;//Local Scratch Registers


outPointStep     RN  3
grpCount         RN  4
dstStep          RN  5
pTmp             RN  4
step             RN  8

;// Neon Registers

dWr             DN  D0.S16
dWi             DN  D1.S16
dXr0            DN  D2.S16
dXi0            DN  D3.S16
dXr1            DN  D4.S16
dXi1            DN  D5.S16
dYr0            DN  D6.S16
dYi0            DN  D7.S16
dYr1            DN  D8.S16
dYi1            DN  D9.S16
qT0             QN  Q5.S32
qT1             QN  Q6.S32


        MACRO
        FFTSTAGE $scaled, $inverse, $name
        
        
        MOV     outPointStep,subFFTSize,LSL #2
        ;// Update grpCount and grpSize rightaway 
        
        MOV     subFFTNum,#1                            ;//after the last stage
        LSL     grpCount,subFFTSize,#1
        
        ;// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount
                               
        SUB      step,outPointStep,#4                   ;// step = -4+outPointStep
        RSB      dstStep,step,#0                        ;// dstStep = -4-outPointStep+8 = -step
        ;//RSB      dstStep,outPointStep,#16
        
        
        ;// Loop on 2 grps at a time for the last stage

grpLoop$name
        VLD2    {dWr[0],dWi[0]},[pTwiddle]!             ;// grp 0
        VLD2    {dWr[1],dWi[1]},[pTwiddle]!             ;// grp 1
        
        ;//VLD2    {dWr,dWi},[pTwiddle],#16        
        
        VLD4    {dXr0[0],dXi0[0],dXr1[0],dXi1[0]},[pSrc]!   ;// grp 0
        VLD4    {dXr0[1],dXi0[1],dXr1[1],dXi1[1]},[pSrc]!   ;// grp 1
        
        
        ;//VLD4    {dXr0,dXi0,dXr1,dXi1},[pSrc],#32
        SUBS    grpCount,grpCount,#4                   ;// grpCount is multiplied by 2 
        
        IF  $inverse
            VMULL   qT0,dXr1,dWr
            VMLAL   qT0,dXi1,dWi                       ;// real part
            VMULL   qT1,dXi1,dWr
            VMLSL   qT1,dXr1,dWi                       ;// imag part
            
        ELSE
            VMULL   qT0,dXr1,dWr
            VMLSL   qT0,dXi1,dWi                       ;// real part
            VMULL   qT1,dXi1,dWr
            VMLAL   qT1,dXr1,dWi                       ;// imag part
        
        ENDIF
        
        VRSHRN  dXr1,qT0,#15
        VRSHRN  dXi1,qT1,#15
        
               
        IF $scaled
        
            VHSUB    dYr0,dXr0,dXr1
            VHSUB    dYi0,dXi0,dXi1
            VHADD    dYr1,dXr0,dXr1
            VHADD    dYi1,dXi0,dXi1
            
        ELSE
        
            VSUB    dYr0,dXr0,dXr1
            VSUB    dYi0,dXi0,dXi1
            VADD    dYr1,dXr0,dXr1
            VADD    dYi1,dXi0,dXi1
            
         
        ENDIF
        
        VST2    {dYr0[0],dYi0[0]},[pDst]!
        VST2    {dYr0[1],dYi0[1]},[pDst],step               ;// step = -4+outPointStep
        
        VST2    {dYr1[0],dYi1[0]},[pDst]!
        VST2    {dYr1[1],dYi1[1]},[pDst],dstStep            ;// dstStep = -4-outPointStep+8 = -step
        
        ;//VST2    {dYr0,dYi0},[pDst],outPointStep
        ;//VST2    {dYr1,dYi1},[pDst],dstStep                  ;// dstStep =  step = -outPointStep + 16
               
        BGT     grpLoop$name
        
        
        ;// Reset and Swap pSrc and pDst for the next stage     
        MOV     pTmp,pDst
        SUB     pDst,pSrc,outPointStep,LSL #1       ;// pDst -= 2*size; pSrc -= 4*size bytes           
        SUB     pSrc,pTmp,outPointStep
        
        ;// Reset pTwiddle for the next stage
        SUB     pTwiddle,pTwiddle,outPointStep      ;// pTwiddle -= 2*size bytes
                
        MEND
        
        
                
        M_START armSP_FFTFwd_CToC_SC16_Radix2_ls_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{FALSE},FWD
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC16_Radix2_ls_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{TRUE},INV
        M_END
 
        
        
        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ls_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{FALSE},FWDSFS
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix2_ls_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{TRUE},INVSFS
        M_END

        
    ENDIF                                                           ;//CORTEXA8
    
     
    END